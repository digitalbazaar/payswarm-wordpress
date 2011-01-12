<?php

/**
 * Adds the administrative pages to the UI if the administrator is logged in.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_add_admin_pages()
{
   // check to see if the PaySwarm options are being set
   $options_not_being_set = !isset($_POST['payswarm_register_button']) ||
      !isset($_POST['payswarm_update_button']);

   // ensure that the administrator is logged in before adding the administrator
   // menus
   if(is_admin())
   {
      // admin actions
      add_action('admin_init', 'payswarm_register_settings');
      add_action('admin_init', 'payswarm_redirect_registration');
      add_action('admin_menu', 'payswarm_create_menu');

      // admin javascript callbacks
      add_action('wp_ajax_payswarm_get_authority_config', 
         'payswarm_get_authority_config');

      // create a warning if the proper settings are not set.
      if($options_not_being_set)
      {
         payswarm_admin_warnings();
      }
   }
}

/**
 * Registers all of the PaySwarm options settings.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_register_settings()
{
   // whitelist the PaySwarm options
   register_setting('payswarm-option-group', 'payswarm_client_config');
   register_setting('payswarm-option-group', 'payswarm_client_id');
   register_setting('payswarm-option-group', 'payswarm_client_secret');
   register_setting('payswarm-option-group', 'payswarm_default_currency');
   register_setting('payswarm-option-group', 'payswarm_destination_account');
   register_setting('payswarm-option-group', 'payswarm_default_license_url');
   register_setting('payswarm-option-group', 'payswarm_authorize_url');
   register_setting('payswarm-option-group', 'payswarm_request_url');
   register_setting('payswarm-option-group', 'payswarm_access_url');
   register_setting('payswarm-option-group', 'payswarm_contracts_url');
   register_setting('payswarm-option-group', 'payswarm_public_key');
   register_setting('payswarm-option-group', 'payswarm_private_key');
}

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
function payswarm_update_options($options)
{
   $rval = true;
   
   // for each option in the list of options, check to see if it exists in
   // the $_POST variable and that it's length is greater than 1
   foreach($options as $option)
   {
      if(isset($_POST[$option]))
      {
         if(strlen($_POST[$option]) > 1)
         {
            update_option($option, $_POST[$option]);
         }
         else
         {
            $rval = false;
         }
      }
      else
      {
         $rval = false;
      }
   }

   return $rval;
}

/**
 * Cleans the '<' and '>' off of a URL from a JSON-LD object.
 * 
 * @return a URL with the '<' and '>' stripped, if they existed.
 */
function payswarm_strip_jsonld_url($url)
{
   $rval = $url;
   $length = strlen($rval);
   
   // check to see if the $url starts and ends with < and > characters, 
   // respectively, and if it does, strip them from the return value
   if($rval[0] === '<' && $rval[$length - 1] === '>')
   {
      $rval = substr($rval, 1, $length - 2);
   }
   
   return $rval;
}

/**
 * Updates the WordPress PaySwarm options given a PaySwarm client configuration
 * retrieved from a PaySwarm authority, a URL-based property in the 
 * configuration and the name of the option to use when saving the value.
 * 
 * @param array $config
 * @param string $property
 * @param string $option
 */
function payswarm_set_client_config_value($config, $property, $option)
{
   $rval = false;
   
   if(array_key_exists($property, $config))
   {
      $value = payswarm_strip_jsonld_url($config[$property]);
      update_option($option, $value);
      
      $rval = true;
   }
   else
   {
      print_r("FAIL: " . $property . ' ' . $option);
   }
   
   return $rval;
}

/**
 * Updates the PaySwarm client configuration given a blob of JSON text.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $json A blob of JSON text that is encoded in JSON-LD format,
 *    usually returned by the PaySwarm Authority after hitting the client
 *    configuration URL.
 *
 * @return true if the update was successful, false otherwise.
 */
function payswarm_update_client_config($json)
{
   $rval = true;
   $client_config = json_decode($json, true);
   
   $authorize_ws = "<http://purl.org/payswarm/webservices#oAuthAuthorize>";
   $request_ws = "<http://purl.org/payswarm/webservices#oAuthRequest>";
   $token_ws = "<http://purl.org/payswarm/webservices#oAuthToken>";
   $contract_ws = "<http://purl.org/payswarm/webservices#oAuthContract>";
   $license_ws = "<http://purl.org/payswarm/webservices#oAuthLicense>";
   $preferences_ws = "<http://purl.org/payswarm/webservices#oAuthPreference>";
   
   if($client_config != NULL)
   {
      $rval &= payswarm_set_client_config_value(
         $client_config, $authorize_ws, 'payswarm_authorize_url');
      $rval &= payswarm_set_client_config_value(
         $client_config, $request_ws, 'payswarm_request_url');
      $rval &= payswarm_set_client_config_value(
         $client_config, $token_ws, 'payswarm_access_url');
      $rval &= payswarm_set_client_config_value(
         $client_config, $contract_ws, 'payswarm_contracts_url');
      $rval &= payswarm_set_client_config_value(
         $client_config, $license_ws, 'payswarm_licenses_url');
      $rval &= payswarm_set_client_config_value(
         $client_config, $preferences_ws, 'payswarm_preferences_url');
   }
   else
   {
      $rval = false;
   }
   
   return $rval;
}

/**
 * Performs a re-direct when a PaySwarm Authority registration attempt is 
 * detected.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_redirect_registration()
{
   if(isset($_POST['payswarm_register_button']))
   {
      // if the PaySwarm options are updated successfully, attempt to perform
      // a registration
      if(payswarm_update_options(array('payswarm_client_config', 
         'payswarm_client_id', 'payswarm_client_secret')))
      {
         // Make the call to the PaySwarm Authority to get the config URL
         $config_url = get_option('payswarm_client_config');

         // FIXME: Ignore SSL - we will want to enable SSL verification
         $response = wp_remote_get($config_url, array('sslverify' => false));

         if(!is_wp_error($response))
         {
            if(payswarm_update_client_config($response['body']))
            {
               wp_redirect(plugins_url() . '/payswarm/payswarm-register.php');
            }
            else
            {
               payswarm_admin_warning(
                  'There was an error reading the client configuration from ' . 
                  'the PaySwarm Config URL. The PaySwarm Authority did not ' . 
                  'provide all of the information that was required for ' . 
                  'configuring this client.');
            }
         }
         else
         {
            payswarm_admin_warning(
               'There was an error when attempting to access the ' . 
               'PaySwarm Config URL. You may want to check to make sure that ' .
               'it is correct.');
         }
      }
   }
}

/**
 * Adds a plugin configuration page to the administration panel.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_create_menu()
{
   //create new top-level menu
   add_submenu_page('plugins.php', 'PaySwarm Configuration', 'PaySwarm', 
      'administrator', 'payswarm', 'payswarm_settings_page');
}

/**
 * Adds a warning to the administration page.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $warning The warning to print to the administration page.
 */
function payswarm_admin_warning($warning) 
{
   echo "
   <div id='payswarm-admin-warning' class='updated fade'>
      <p>". __($warning) .
     __(' You still need to ' .
       '<a href="plugins.php?page=payswarm">configure PaySwarm</a>.') .
   "  </p>
   </div>";
}

/**
 * Generates any administration warnings dealing with PaySwarm configuration.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_admin_warnings() 
{
   $options_not_being_set = !isset($_POST['payswarm_client_config']) ||
      !isset($_POST['payswarm_client_id']) ||
      !isset($_POST['payswarm_client_secret']) ||
      !isset($_POST['payswarm_default_currency']) ||
      !isset($_POST['payswarm_destination_account']) ||
      !isset($_POST['payswarm_default_license_url']) ||
      !isset($_POST['payswarm_authorize_url']) ||
      !isset($_POST['payswarm_request_url']) ||
      !isset($_POST['payswarm_access_url']) ||
      !isset($_POST['payswarm_contracts_url']) ||
      !isset($_POST['payswarm_public_key']) ||
      !isset($_POST['payswarm_private_key']);
   $options_nonexistant = !get_option('payswarm_client_config') ||
      !get_option('payswarm_client_id') ||
      !get_option('payswarm_client_secret') ||
      !get_option('payswarm_default_currency') ||
      !get_option('payswarm_destination_account') ||
      !get_option('payswarm_default_license_url') ||
      !get_option('payswarm_authorize_url') ||
      !get_option('payswarm_request_url') ||
      !get_option('payswarm_access_url') ||
      !get_option('payswarm_contracts_url') ||
      !get_option('payswarm_public_key') ||
      !get_option('payswarm_private_key');
   $options_invalid = strlen(get_option('payswarm_client_config')) < 1 ||
      strlen(get_option('payswarm_client_id')) < 1 ||
      strlen(get_option('payswarm_client_secret')) < 1 ||
      strlen(get_option('payswarm_default_currency')) < 1 ||
      strlen(get_option('payswarm_destination_account')) < 1 ||
      strlen(get_option('payswarm_default_license_url')) < 1 ||
      strlen(get_option('payswarm_authorize_url')) < 1 ||
      strlen(get_option('payswarm_request_url')) < 1 ||
      strlen(get_option('payswarm_access_url')) < 1 ||
      strlen(get_option('payswarm_contracts_url')) < 1 ||
      strlen(get_option('payswarm_public_key')) < 1 ||
      strlen(get_option('payswarm_private_key')) < 1;

   // output a warning if any part of the PaySwarm configuration is not set
   if(($options_nonexistant && $options_not_being_set) || $options_invalid)
   {
      function psaw_config()
      {
         payswarm_admin_warning(
            'A number of PaySwarm options are not correct.');
      }
      add_action('admin_notices', 'psaw_config');
   }
}

/**
 * Generates a public-private X509 encoded keys.
 * 
 * @package payswarm
 * @since 1.0
 * 
 * @return Array containing two keys 'public' and 'private' each with the
 *    public and private keys encoded in X509 format.
 */
function payswarm_generate_keypair()
{
   $rval = array();

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
   
   return $rval;
}

/**
 * Displays the PaySwarm settings page to the administrator.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_settings_page()
{
   $all_options_set = isset($_POST['payswarm_client_config']) &&
      isset($_POST['payswarm_client_id']) &&
      isset($_POST['payswarm_client_secret']) &&
      isset($_POST['payswarm_default_currency']) &&
      isset($_POST['payswarm_destination_account']) &&
      isset($_POST['payswarm_default_license_url']) &&
      isset($_POST['payswarm_authorize_url']) &&
      isset($_POST['payswarm_request_url']) &&
      isset($_POST['payswarm_access_url']) &&
      isset($_POST['payswarm_contracts_url']) &&
      isset($_POST['payswarm_public_key']) &&
      isset($_POST['payswarm_private_key']);

   // update the consumer key, secret and default currency if they are set
   if($all_options_set)
   {
      check_admin_referer('payswarm-save-config', 'payswarm-nonce');
      $authority = $_POST['payswarm_client_config'];
      $client_id = $_POST['payswarm_client_id'];
      $client_secret = $_POST['payswarm_client_secret'];
      $default_currency = $_POST['payswarm_default_currency'];
      $default_account = $_POST['payswarm_destination_account'];
      $default_license_url = $_POST['payswarm_default_license_url'];
      $default_license_hash = 'INVALID';
      $authorize_url = $_POST['payswarm_authorize_url'];
      $request_url = $_POST['payswarm_request_url'];
      $access_url = $_POST['payswarm_access_url'];
      $contracts_url = $_POST['payswarm_contracts_url'];
      $public_key = $_POST['payswarm_public_key'];
      $private_key = $_POST['payswarm_private_key'];

      // FIXME: calculate the default license data hash by requesting that
      // the hash is generated by the PaySwarm Authority using the PaySwarm
      // Client ID and PaySwarm Client Secret.
      $http = new WP_Http;
      $response = $http->request($default_license_url);
      if(is_array($response) && $response['response']['code'] == 200 && 
         isset($response['body']))
      {
         $default_license_hash = sha1($response['body']);
      }

      // FIXME: Update the public key information at the PaySwarm Authority
      $webid_url = "http://example.org/people/bob#webid";
      $public_key_url = "http://example.org/people/bob#key-5";

      update_option('payswarm_client_config', $authority);
      update_option('payswarm_client_id', $client_id);
      update_option('payswarm_client_secret', $client_secret);
      update_option('payswarm_default_currency', $default_currency);
      update_option('payswarm_destination_account', $default_account);
      update_option('payswarm_default_license_url', $default_license_url);
      update_option('payswarm_default_license_hash', $default_license_hash);
      update_option('payswarm_authorize_url', $authorize_url);
      update_option('payswarm_request_url', $request_url);
      update_option('payswarm_access_url', $access_url);
      update_option('payswarm_contracts_url', $contracts_url);
      update_option('payswarm_public_key', $public_key);
      update_option('payswarm_private_key', $private_key);
      update_option('payswarm_webid_url', $webid_url);
      update_option('payswarm_public_key_url', $public_key_url);
   }

?>
<div class="wrap">
<h2>PaySwarm Configuration</h2>

<p>
PaySwarm enables you to sell your articles, photos and other content directly
to your website visitors. You can charge as little as a penny per article,
or up to hundreds of dollars for your content. 
To use PaySwarm, you must first go to a
<a href="http://dev.payswarm.com:19100">PaySwarm Provider</a> and create an
account. Once you have an account, you will be able to generate a
<strong>PaySwarm Client ID</strong> and a
<strong>PaySwarm Client Secret</strong>, which you must enter below to
activate PaySwarm on your site.
</p>

<form method="post" action="">
    <?php settings_fields('payswarm-settings-group'); ?>
    <table class="form-table">

        <tr valign="top">
          <td scope="row" colspan="2">
            <h3><?php _e('Basic PaySwarm Settings') ?></h3>
            <p>You must register with a PaySwarm Authority first. You must
            then generate a PaySwarm Client ID and a PaySwarm Client Secret
            on that PaySwarm Authority. Enter the information that you generate
            via your PaySwarm Authority below.
            </p>
          </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Config URL') ?></th>
        <td><input type="text" size="64" 
          id="payswarm_client_config" name="payswarm_client_config" 
          onblur="javascript:updateAuthorityConfig();"
          value="<?php echo get_option('payswarm_client_config'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Client ID') ?></th>
        <td><input type="text" size="64" name="payswarm_client_id" 
          value="<?php echo get_option('payswarm_client_id'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Client Secret') ?></th>
        <td><input type="text" size="64" name="payswarm_client_secret" 
          value="<?php echo get_option('payswarm_client_secret'); ?>" /></td>
        </tr>

        <tr valign="top">
        <td scope="row" colspan="2">
           <input type="submit" class="button-secondary"
              name="payswarm_register_button"
              value="<?php _e('Register this site') ?>" />
        </td>
        </tr>

        <tr valign="top">
        <td scope="row" colspan="2">
          <h3><?php _e('Advanced: PaySwarm Authority Web Services') ?></h3>
          <p>You can specify different PaySwarm Authority Web Service URLs
          below. Warning: These are advanced settings and entering the wrong
          URLs will ensure that your customers will not be able to buy
          articles on your website.
          </p>
        </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Authorize Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_authorize_url" name="payswarm_authorize_url" 
          value="<?php echo get_option('payswarm_authorize_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Request Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_request_url" name="payswarm_request_url" 
          value="<?php echo get_option('payswarm_request_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Access Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_access_url" name="payswarm_access_url" 
          value="<?php echo get_option('payswarm_access_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Preferences Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_preferences_url" name="payswarm_preferences_url" 
          value="<?php echo get_option('payswarm_preferences_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('License Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_licenses_url" name="payswarm_licenses_url" 
          value="<?php echo get_option('payswarm_licenses_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Contracts Service') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_contracts_url" name="payswarm_contracts_url" 
          value="<?php echo get_option('payswarm_contracts_url'); ?>" /></td>
        </tr>

        <tr valign="top">
        <td scope="row" colspan="2">
          <p>PaySwarm uses 
             <a href="http://en.wikipedia.org/wiki/Digital_signature"
                >digital signatures</a> to ensure that posts that you
             sell can be verified by PaySwarm Authorities. Your signature
             keys are listed below. Do not change either of them unless you know
             what you are doing.
          </p>
        </td>
        </tr>

<?php 
   $new_keys = false;
   $public_key = get_option('payswarm_public_key');
   $private_key = get_option('payswarm_private_key');
   if($public_key == '' or $private_key == '')
   {
      $keypair = payswarm_generate_keypair();
      $public_key = $keypair['public'];
      $private_key = $keypair['private'];
      $new_keys = true;
   }

   echo "<tr valign=\"top\">
        <th scope=\"row\">" . __('Public Key') . "</th>
        <td><textarea cols=\"80\" rows=\"6\" 
          id=\"payswarm_public_key\" name=\"payswarm_public_key\"
          >$public_key</textarea></td>
        </tr>

        <tr valign=\"top\">
        <th scope=\"row\">" . __('Private Key') . "</th>
        <td><textarea cols=\"80\" rows=\"6\" 
          id=\"payswarm_private_key\" name=\"payswarm_private_key\"
          >$private_key</textarea></td>
        </tr>";

    if($new_keys)
    {
       echo '<tr><th></th><td>' . 
          __('Note: New digital signature keys have been created for you, ' .
             'please click <strong>Save Changes</strong>.') . '</td></tr>';
    }
?>

        <tr valign="top">
        <td scope="row" colspan="2">
          <h3><?php _e('Step 3: Financial Account and License Defaults') ?></h3>
          <p>The default article license is used to convey a set of license
          rights to the people that purchase your articles. The URL listed
          below should return a plain text file that outlines the permissions of 
          the buyer of the article as well as the responsibilities of the seller 
          of the article.
          </p>
        </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Default Currency') ?></th>
        <td>
          <select name="payswarm_default_currency">
            <option value="USD">$ - Dollars (USD)</option>
          </select>
        </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Default Financial Account') ?></th>
        <td>
          <input type="text" size="128" name="payswarm_destination_account" 
            value="<?php echo get_option('payswarm_destination_account'); ?>" />
        </td>

        <tr valign="top">
        <td scope="row" colspan="2">
          <p>The default article license is used to convey a set of license
          rights to the people that purchase your articles. The URL listed
          below should return a plain text file that outlines the permissions of 
          the buyer of the article as well as the responsibilities of the seller 
          of the article.
          </p>
        </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Default License') ?></th>
        <td><input type="text" size="128" 
          id="payswarm_default_license_url" name="payswarm_default_license_url" 
          value="<?php echo get_option('payswarm_default_license_url'); ?>" />
        <?php 
        if(get_option('payswarm_default_license_hash') !== false)
        {
           echo '<p>Default License ID: ' . 
           get_option('payswarm_default_license_hash');
        }
        ?>
        </p>
        </td>
        </tr>

        <?php wp_nonce_field('payswarm-save-config', 'payswarm-nonce') ?>

    </table>



    <p class="submit">
    <input type="submit" class="button-primary" 
       name="payswarm_update_button"
       value="<?php _e('Save PaySwarm Configuration') ?>" />
    </p>

</form>
</div>
<?php if(!empty($_POST['payswarm_client_id'])) : ?>
<div id="message" class="updated fade">
  <p><strong><?php _e('PaySwarm options saved.') ?></strong></p>
</div>
<?php endif; ?>
<?php 
}
?>
