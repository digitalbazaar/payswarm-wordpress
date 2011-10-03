<?php
require_once('jsonld.php');

/**
 * Updates the given options that exist and are valid in the $_POST environment.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $options An array of strings that are the names of the option
 *    values that exist in the $_POST environment variable.
 * @return true if setting all options was successful, false otherwise.
 */
function payswarm_config_update($options = NULL)
{
   $rval = true;

   if($options !== NULL)
   {
      // for each option in the list of options, check to see if it exists in
      // the $_POST variable and that it's length is greater than 1
      foreach($options as $option)
      {
         if(isset($_POST[$option]) && strlen($_POST[$option]) > 1)
         {
            update_option($option, $_POST[$option]);
         }
         else
         {
            $rval = false;
         }
      }
   }

   // set resign_if_before date
   $now = date_format(date_create(), 'Y-m-d\TH:i:s\Z');
   update_option('payswarm_resign_if_before', $now);

   return $rval;
}

/**
 * Updates the WordPress PaySwarm options given a PaySwarm client configuration
 * retrieved from a PaySwarm authority, a URL-based property in the
 * configuration and the name of the option to use when saving the value.
 *
 * @param array $config an associative array of config values returned by a
 *    PaySwarm client config JSON-LD service
 * @param string $property a URL string specifying the property in the JSON-LD
 *    object.
 * @param string $option the name of the WordPress option to update.
 */
function payswarm_config_set_value($config, $property, $option)
{
   $rval = false;

   $config = (array)$config;
   if(array_key_exists($property, $config))
   {
      update_option($option, $config[$property]);
      $rval = true;
   }

   return $rval;
}

/**
 * Updates the PaySwarm client configuration given a blob of JSON text.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $jsonld A blob of JSON text that is encoded in JSON-LD format,
 *    usually returned by the PaySwarm Authority after hitting the
 *    configuration URL.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_endpoints($jsonld)
{
   $rval = true;
   $client_config = json_decode($jsonld);

   $rval =
      ($client_config !== NULL) &&
      payswarm_config_set_value(
         $client_config, "ps:registrationService", 
         'payswarm_registration_url') &&
      payswarm_config_set_value(
         $client_config, "ps:contractsService", 'payswarm_contracts_url') &&
      payswarm_config_set_value(
         $client_config, "ps:licenseService", 'payswarm_licenses_url') &&
      payswarm_config_set_value(
         $client_config, "ps:publicKeyService", 'payswarm_keys_url');

   return $rval;
}

/**
 * Updates the PaySwarm public/private key information associated with this
 * installation of WordPress.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $keys The public/private keypair that was registered. The
 *    associative array should have to keys in it - 'public' and 'private' for
 *    the public key and the private key, respectively.
 * @param array $jsonld The registration information for the public key, which
 *    is a JSON-LD object that contains the public key URL and the WebID URL
 *    that owns the key.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_keys($keys, $jsonld)
{
   $rval = true;
   $public_key_registration = (array)jsonld_expand(json_decode($jsonld));
   $cert_identity = "http://www.w3.org/ns/auth/cert#identity";

   $rval =
      ($public_key_registration !== NULL) &&
      payswarm_config_set_value(
         $public_key_registration, '@subject', 'payswarm_public_key_url') &&
      payswarm_config_set_value(
         $public_key_registration, $cert_identity, 'payswarm_webid_url');

   if($rval)
   {
      update_option('payswarm_public_key', $keys['public']);
      update_option('payswarm_private_key', $keys['private']);
   }

   return $rval;
}

/**
 * Updates the PaySwarm preferences associated with this WordPress installation.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $jsonld The preferences JSON-LD markup received from the
 *    PaySwarm preferences webservice.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_preferences($jsonld)
{
   $rval = true;
   $preferences = (array)jsonld_expand(json_decode($jsonld));

   $currency_url = "http://purl.org/payswarm/preferences#currency";
   $account_url = "http://purl.org/payswarm#account";
   $license_url = "http://purl.org/payswarm/preferences#license";
   $license_hash_url = "http://purl.org/payswarm#licenseHash";

   $rval =
      ($preferences !== NULL) &&
      payswarm_config_set_value(
         $preferences[$license_url][0],
         '@subject', 'payswarm_default_license_url') &&
      payswarm_config_set_value(
         $preferences[$license_url][0],
         $license_hash_url, 'payswarm_default_license_hash') &&
      payswarm_config_set_value(
         $preferences, $currency_url, 'payswarm_default_currency') &&
      payswarm_config_set_value(
         $preferences[$account_url][0],
         '@subject', 'payswarm_destination_account');

   return $rval;
}

?>
