<?php

require_once('jsonld.php');
require_once('payswarm-utils.inc');

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
 * Gets the config URL for the given PaySwarm Authority config URL.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $pa_url the base URL to the PaySwarm Authority.
 *
 * @return string the PaySwarm Authority config URL.
 */
function payswarm_config_get_pa_config_url($pa_url)
{
   preg_match('@(http://|https://)?([^/]+)@i', $pa_url, $matches);
   $hostPort = $matches[2];
   return "https://$hostPort/payswarm-v1-config";
}

/**
 * Fetches the config for the given PaySwarm Authority config URL.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $config_url the config URL for the PaySwarm Authority.
 *
 * @return string the JSON-LD config object returned by the PaySwarm Authority
 *         or false if an error occurred.
 */
function payswarm_config_get_pa_config($config_url)
{
   $rval = false;

   // fetch the PA config information
   $response = wp_remote_get(
      $config_url, array('sslverify' => PAYSWARM_SSL_ENABLED));
   if(!is_wp_error($response) and
      ($cfg = payswarm_jsonld_decode($response['body'])) !== null)
   {
      // FIXME: uncomment once remote @context URL is defined
      /*
      $frame = (object)array(
         '@context' => payswarm_create_default_jsonld_context(),
         'ps:paymentService' => '',
         'ps:vendorRegistrationService' => '',
         'ps:publicKey' => '',
         'ps:contractsService' => '',
         'ps:licensesService' => '',
         'ps:publicKeysService' => ''
      );
      $rval = jsonld_frame($cfg, $frame);
      */
      $rval = $cfg;
      if($rval === null)
      {
         $rval = false;
      }
   }

   return $rval;
}

/**
 * Updates the PaySwarm client configuration.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $cfg a JSON-LD config object returned by the PaySwarm
 *           Authority after hitting the configuration URL.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_config_endpoints($cfg)
{
   $rval = true;

   $rval =
      ($cfg !== NULL) &&
      // Form endpoints
      payswarm_config_set_value(
         $cfg, 'ps:paymentService',
         'payswarm_payment_url') &&
      payswarm_config_set_value(
         $cfg, 'ps:vendorRegistrationService',
         'payswarm_registration_url') &&
      // Authority details
      payswarm_config_set_value(
         $cfg, 'ps:publicKey', 'payswarm_authority_public_key_url') &&
      // API services
      payswarm_config_set_value(
         $cfg, 'ps:contractsService', 'payswarm_contracts_url') &&
      payswarm_config_set_value(
         $cfg, 'ps:licensesService', 'payswarm_licenses_url') &&
      payswarm_config_set_value(
         $cfg, 'ps:publicKeysService', 'payswarm_keys_url');

   return $rval;
}

/**
 * Updates the PaySwarm Authority defined preferences associated with this
 * WordPress installation.
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
         $preferences, 'ps:owner', 'payswarm_vendor_url');
      payswarm_config_set_value(
         $preferences, 'com:currency', 'payswarm_default_currency') &&
      payswarm_config_set_value(
         $preferences, 'com:destination', 'payswarm_destination_account') &&
      payswarm_config_set_value(
         $preferences, 'sec:publicKey', 'payswarm_public_key_url');

   return $rval;
}

?>
