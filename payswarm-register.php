<?php
// NOTE: DO not move this block of code, we need to extract the encrypted
// message if it exists before including wp-config.php
if(isset($_POST["encrypted-message"]))
{
   $json_message = $_POST["encrypted-message"];
}

// NOTE: When you require wp-config.php, magic quotes is turned on to
// modify POST parameters
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');
require_once('payswarm-client.inc');

// if no message was posted, redirect to PA for registration
if(!isset($json_message))
{
   // generate/fetch public key
   $keys = array();
   if(!get_option('payswarm_public_key') ||
      get_option('payswarm_key_overwrite') === 'true')
   {
      // generate the public/private keypair
      $keys = payswarm_generate_keypair(false);
      payswarm_config_keys($keys);
   }
   else
   {
      $keys["public"] = get_option('payswarm_public_key');
   }

   // register the keypair with the PaySwarm Authority
   $reg_base_url = get_option('payswarm_registration_url');

   // generate the registration re-direct URL
   $callback_url = plugins_url() . '/payswarm/payswarm-register.php' .
      "?nonce=" . payswarm_create_message_nonce();
   $registration_url = $reg_base_url .
      '?public-key=' . urlencode($keys['public']) .
      '&registration-callback=' . urlencode($callback_url);

   // re-direct the user agent to the PaySwarm Authority registration URL
   header("HTTP/1.1 303 See Other");
   header("Location: $registration_url");
}
// handle posted message
else
{
   // decode json-encoded, encrypted message
   $msg = payswarm_decode_payswarm_authority_message($json_message);

   // update the vendor preferences
   if($msg !== false)
   {
      payswarm_config_preferences($msg);
      header('Location: ' . admin_url() . 'plugins.php?page=payswarm');
   }
   else
   {
      // FIXME: Create proper error message
      echo "Failed to set PaySwarm configuration preferences.";
   }
}

/**
 * Gets the PaySwarm OAuth client configuration for the given user.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_registration_config($options)
{
   // FIXME: consider moving into payswarm oauth function(s)?
   try
   {
      // setup OAuth client
      $oauth = payswarm_create_oauth_client(
         $options['client_id'], $options['client_secret']);
      $token = $options['token'];
      $oauth->setToken($token['token'], $token['secret']);

      try
      {
         $config_url = get_option('payswarm_client_config');
         $oauth->setAuthType(OAUTH_AUTH_TYPE_URI);
         $oauth->fetch($config_url);
      }
      catch(OAuthException $E)
      {
         // FIXME: can this produce an infinite redirect problem?

         // if request failed, then assume token has expired, remove token
         // and try again
         payswarm_database_delete_token($options['token']);
         wp_redirect(plugins_url() . '/payswarm/payswarm-register.php');
      }

      // get PaySwarm endpoints from response
      $json = $oauth->getLastResponse();
      if(!payswarm_config_endpoints($json))
      {
         die("Error: Failed to set configuration endpoints. " .
            "Response received from PaySwarm Authority: " .
            htmlentities($json));
      }

      // generate a key pair to send to payswarm authority, only overwrite
      // existing key if option specifies it
      $keygen = (get_option('payswarm_key_overwrite') === 'true');
      $keys = payswarm_generate_keypair(!$keygen);

      // register the public/private keypair
      $post_data = array("public_key" => $keys['public']);
      if($keys['public_key_url'] !== '')
      {
         $post_data['public_key_url'] = $keys['public_key_url'];
      }
      try
      {
         $keys_url = get_option('payswarm_keys_url');
         $oauth->setAuthType(OAUTH_AUTH_TYPE_FORM);
         $oauth->fetch($keys_url, $post_data);
         $key_registration_info = $oauth->getLastResponse();
         if(!payswarm_config_keys($keys, $key_registration_info))
         {
            die("Error: Failed to set public/private key configuration. " .
               "Response received from PaySwarm Authority: " .
               htmlentities($key_registration_info));
         }
      }
      catch(OAuthException $E)
      {
         // if exception is duplicate ID, ignore
         $err = json_decode($E->lastResponse, true);
         if(!($err['type'] === 'payswarm.website.AddPublicKeyFailed' and
            $err['cause']['type'] === 'payswarm.user.DuplicatePublicKey'))
         {
            throw $E;
         }
      }

      // get the individualized PaySwarm preferences
      $preferences_url = get_option('payswarm_preferences_url');
      $oauth->setAuthType(OAUTH_AUTH_TYPE_URI);
      $oauth->fetch($preferences_url);
      $preferences = $oauth->getLastResponse();
      if(!payswarm_config_preferences($preferences))
      {
         die("Error: Failed to set PaySwarm configuration preferences. " .
            "Response received from PaySwarm Authority: " .
            htmlentities($preferences));
      }
      else
      {
         // check to see if the default price/rate are set, if not, set them
         $default_price = get_option('payswarm_default_price');
         $default_auth_rate = get_option('payswarm_default_auth_rate');
         if(!is_numeric($default_price))
         {
            $default_price = '0.05';
         }
         if(!is_numeric($default_auth_rate))
         {
            $default_auth_rate = '10';
         }
         update_option('payswarm_default_price',
            sprintf('%1.07f', floatval($default_price)));
         update_option('payswarm_default_auth_rate',
            sprintf('%1.07f', floatval($default_auth_rate)));

         // FIXME: we need the default prices and auth rates to propagate
         // to posts so that they're prices will be updated -- at least in
         // the case that the user prefers it?
      }

      // notify that config has been updated
      payswarm_config_update();

      // redirect to admin page
      header('Location: ' . admin_url() . 'plugins.php?page=payswarm');
   }
   catch(OAuthException $E)
   {
      $err = json_decode($E->lastResponse);
      print_r('<pre>' . $E . "\nError details: \n" .
         print_r($err, true) . '</pre>');
   }
}

/**
 * Called when access is denied to do PaySwarm registration.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_access_denied($options)
{
   // FIXME: Unfortunately, this generates a PHP Notice error for
   // WP_Query::$is_paged not being defined. Need to figure out which file
   // declares that variable.
   get_header();

   echo '
<div class="category-uncategorized">
  <h2 class="entry-title">Access Denied when Registering</h2>
  <div class="entry-content">
    <p>
      Access to the PaySwarm registration information was denied because this
      website was not allowed to access your PaySwarm account. This usually
      happens because you did not allow this website to access your
      PaySwarm account information by not assigning it a Registration Token.
    </p>

    <p><a href="' . site_url() . "/wp-admin/plugins.php?page=payswarm" .
      '">Go back to the administrative page</a>.</p>
  </div>
</div>';

   get_footer();
}

/**
 * Generates a public-private X509 encoded keys or gets an existing pair.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Boolean reuse true to reuse the existing keypair from the config,
 *           false not to.
 *
 * @return Array containing two keys 'public' and 'private' each with the
 *    public and private keys encoded in PEM X509 format, and 'public_key_url'
 *    will either be set to '' for new keys or the old URL if reusing keys.
 */
function payswarm_generate_keypair($reuse)
{
   $rval = array();

   // reuse existing key, do not keygen
   if($reuse)
   {
      $rval['public'] = get_option('payswarm_public_key');
      $rval['private'] = get_option('payswarm_private_key');
      $rval['public_key_url'] = get_option('payswarm_public_key_url');
      if($rval['public'] === '' or $rval['private'] === '' or
         $rval['public_key_url'] === '')
      {
         $reuse = false;
      }
   }

   if(!$reuse)
   {
      // Create the keypair
      $keypair = openssl_pkey_new();

      // Get private key
      openssl_pkey_export($keypair, $privkey);

      // Get public key
      $pubkey = openssl_pkey_get_details($keypair);
      $pubkey = $pubkey["key"];

      // free the keypair
      openssl_free_key($keypair);

      $rval['public'] = $pubkey;
      $rval['private'] = $privkey;
      $rval['public_key_url'] = '';
   }

   return $rval;
}

?>
