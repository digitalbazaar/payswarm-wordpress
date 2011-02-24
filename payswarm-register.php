<?php
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');

// Force access to happen through SSL
payswarm_force_ssl();

require_once('payswarm-session.inc');
require_once('payswarm-database.inc');
require_once('payswarm-oauth.inc');

// the session ID that is associated with this request
$session = 'payswarm-registration';

// get the scope that is associated with this request
$scope = 'payswarm-registration';

// retrieve the PaySwarm token, creating it if it doesn't already exist
$ptoken = payswarm_database_get_token($session, $scope, true);

// If we are authorizing, then there should be an oauth_token, if not, start
// the process over.
if($ptoken['state'] === 'authorizing' && !isset($_GET['oauth_token']))
{
   $ptoken['state'] = 'initializing';
}

try
{
   // setup the OAuth client
   $client_id = get_option('payswarm_client_id');
   $client_secret = get_option('payswarm_client_secret');
   $oauth = new OAuth(
      $client_id, $client_secret, OAUTH_SIG_METHOD_HMACSHA1,
      OAUTH_AUTH_TYPE_FORM);

   // FIXME: Disable debug output for OAuth for production software
   $oauth->enableDebug();

   // FIXME: Enable SSL checks for production software
   $oauth->disableSSLChecks();

   // check the state of the payment token
   if($ptoken['state'] === 'initializing')
   {
      $request_url = get_option('payswarm_request_url') . "?scope=$scope";
      $details = array('balance' => '0.0');

      //die("INITIAL");
      payswarm_oauth1_initialize(
         $oauth, $session, $scope, $request_url, $details);
      die("INITIAL 2");
   }
   else if($ptoken['state'] === 'authorizing')
   {
      // State 1 - Handle callback from PaySwarm
      if(array_key_exists('oauth_verifier', $_GET))
      {
         // get and store an access token
         $access_url = get_option('payswarm_access_url');
         $oauth->setToken($_GET['oauth_token'], $ptoken['secret']);
         $details = array('balance' => '0.0');

         payswarm_oauth1_authorize(
            $oauth, $session, $scope, $access_url, $details);
      }
      else
      {
         // if access was denied, print out an appropriate error
         payswarm_registration_denied($post);
      }
   }
   else if($ptoken['scope'] === 'payswarm-registration' &&
      $ptoken['state'] === 'valid')
   {
      $oauth = new OAuth($client_id, $client_secret, OAUTH_SIG_METHOD_HMACSHA1);
      // FIXME: Disable debug output for OAuth for production software
      // FIXME: Enable SSL checks for production software
      $oauth->enableDebug();
      $oauth->disableSSLChecks();

      // setup the PaySwarm token and secret in preparation for making OAuth
      // calls
      $oauth->setToken($ptoken['token'], $ptoken['secret']);

      // Make the call to the PaySwarm Authority to get the complete
      // PaySwarm endpoint information
      $config_url = get_option('payswarm_client_config');
      try
      {
         $oauth->fetch($config_url, array(), OAUTH_HTTP_METHOD_GET);
      }
      catch(OAuthException $E)
      {
         // if the OAuth request failed, then the PaySwarm token has
         // expired. Remove the token and attempt the registration again.
         payswarm_database_delete_token($ptoken);
         wp_redirect(plugins_url() . '/payswarm/payswarm-register.php');
      }

      // store the PaySwarm endpoints
      $response_info = $oauth->getLastResponseInfo();
      $json = $oauth->getLastResponse();
      $success = payswarm_config_endpoints($json);

      $key_registration_info = "{}";
      if($success)
      {
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
         $keys_url = get_option('payswarm_keys_url');
         $oauth->fetch($keys_url, $post_data, OAUTH_HTTP_METHOD_POST);
         $key_registration_info = $oauth->getLastResponse();
         $success = payswarm_config_keys($keys, $key_registration_info);
      }
      else
      {
         die("Error: Failed to set configuration endpoints. " .
            "Response received from PaySwarm Authority: " .
            htmlentities($json));
      }

      $preferences = "{}";
      if($success)
      {
         // Retrieve the individualized PaySwarm preferences
         $preferences_url = get_option('payswarm_preferences_url');
         $oauth->fetch($preferences_url);
         $preferences = $oauth->getLastResponse();
         $success = payswarm_config_preferences($preferences);
      }
      else
      {
         die("Error: Failed to set public/private key configuration. " .
            "Response received from PaySwarm Authority: " .
            htmlentities($key_registration_info));
      }

      if($success)
      {
          header('Location: ' . admin_url() . 'plugins.php?page=payswarm');
      }
      else
      {
         die("Error: Failed to set PaySwarm configuration preferences. " .
            "Response received from PaySwarm Authority: " .
            htmlentities($preferences));
      }
   }
}
catch(OAuthException $E)
{
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" .
      print_r($err, true) . '</pre>');
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

function payswarm_access_denied($post)
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
?>
