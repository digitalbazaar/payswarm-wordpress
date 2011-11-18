<?php

// NOTE: DO not move this block of code, we need to extract the encrypted
// message if it exists before including wp-config.php
if(isset($_POST["encrypted-message"]))
{
   $json_message = $_POST["encrypted-message"];

   // make sure to remove magic quotes if in use
   if(get_magic_quotes_gpc())
   {
      $json_message = stripcslashes($json_message);
   }
}

// NOTE: When you require wp-config.php, magic quotes is turned on to
// modify POST parameters
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');
require_once('payswarm-security.inc');

// if no message was posted, redirect to PaySwarm Authority for registration
if(!isset($json_message))
{
   // get the key pair to be registered
   $keys = payswarm_get_key_pair();

   // generate the PA registration re-direct URL
   $callback_url = plugins_url() . '/payswarm/payswarm-register.php';
   $registration_url = get_option('payswarm_registration_url') .
      '?response-nonce=' . payswarm_create_message_nonce() .
      '&public-key=' . urlencode($keys['public']) .
      '&registration-callback=' . urlencode($callback_url);

   // re-direct the user agent to the PaySwarm Authority registration URL
   header('HTTP/1.1 303 See Other');
   header("Location: $registration_url");
}
// handle posted registration response message
else
{
   // FIXME: catch exceptions and show appropriate errors to user
   // decode json-encoded, encrypted message
   $msg = payswarm_decode_payswarm_authority_message($json_message);

   // check message type
   if(payswarm_jsonld_has_type($msg, 'err:Error'))
   {
      throw new Exception('PaySwarm Registration Exception: ' .
         $msg->{'err:message'});
   }
   else if(!payswarm_jsonld_has_type($msg, 'ps:Preferences'))
   {
      throw new Exception('PaySwarm Registration Exception: ' .
         'Invalid registration response from PaySwarm Authority.');
   }

   // update the vendor preferences
   payswarm_config_preferences($msg);

   // show admin page
   $url = admin_url() . 'plugins.php?page=payswarm';
   echo "
      <html><body>
      <script type=\"text/javascript\">
      if(window.opener === null)
      {
         window.location = '$url';
      }
      else
      {
         window.close();
         window.opener.location = '$url';
      }
      </script>
      </body></html>";
   exit(0);
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
 * Gets the public-private X.509 PEM-encoded key pair, generating it if
 * necessary or if requested by the payswarm_key_overwrite option.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return Array containing two keys 'public' and 'private' each with the
 *    public and private keys encoded in PEM X.509 format, and 'public_key_url'
 *    will either be set to '' for new keys or the existing URL if reusing keys.
 */
function payswarm_get_key_pair()
{
   // generate key pair if one does not exist or overwrite requested
   if(get_option('payswarm_private_key') === false or
      get_option('payswarm_public_key') === false or
      get_option('payswarm_key_overwrite') === 'true')
   {
      // generate the key pair
      $keypair = openssl_pkey_new();

      // get private key and public key in PEM format
      openssl_pkey_export($keypair, $privkey);
      $pubkey = openssl_pkey_get_details($keypair);
      $pubkey = $pubkey['key'];

      // free the key pair
      openssl_free_key($keypair);

      // store PEM keys
      update_option('payswarm_private_key', $privkey);
      update_option('payswarm_public_key', $pubkey);
   }

   // return PEM keys and URL
   return array(
      'private' => get_option('payswarm_private_key'),
      'public' => get_option('payswarm_public_key'),
      'public_key_url' => get_option('payswarm_public_key_url', '')
   );
}

?>
