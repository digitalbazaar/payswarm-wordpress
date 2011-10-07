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
         $client_config, "ps:vendorRegistrationService", 
         'payswarm_registration_url') &&
      payswarm_config_set_value(
         $client_config, "ps:publicKey", 'payswarm_authority_public_key_url') &&
      payswarm_config_set_value(
         $client_config, "ps:contractsService", 'payswarm_contracts_url') &&
      payswarm_config_set_value(
         $client_config, "ps:licenseService", 'payswarm_licenses_url') &&
      payswarm_config_set_value(
         $client_config, "ps:publicKeyService", 'payswarm_keys_url');

   // reset the PaySwarm Authority public key information
   delete_option('payswarm_authority_public_key_expiration');
   delete_option('payswarm_authority_public_key');

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
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_keys($keys)
{
   update_option('payswarm_public_key', $keys['public']);
   update_option('payswarm_private_key', $keys['private']);

   return $rval;
}

/**
 * Updates the PaySwarm preferences associated with this WordPress installation.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $preferences An associative array of preferences returned by
 *                           the registration service.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_preferences($preferences)
{
   $rval = true;

   $rval =
      ($preferences !== NULL) &&
      payswarm_config_set_value(
         $preferences, 'ps:license', 'payswarm_default_license_url') &&
      payswarm_config_set_value(
         $preferences, 'ps:licenseHash', 'payswarm_default_license_hash') &&
      payswarm_config_set_value(
         $preferences, 'com:currency', 'payswarm_default_currency') &&
      payswarm_config_set_value(
         $preferences, 'com:minimumAmount', 'payswarm_default_price') &&
      payswarm_config_set_value(
         $preferences, 'com:destination', 'payswarm_destination_account') &&
      payswarm_config_set_value(
         $preferences, 'com:maximumRate', 'payswarm_default_auth_rate') &&
      payswarm_config_set_value(
         $preferences, 'sec:publicKey', 'payswarm_public_key_url');

   return $rval;
}

?>
