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
      // Force management to happen through SSL
      payswarm_force_ssl();

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
   register_setting('payswarm-option-group', 'payswarm_config_url');
   register_setting('payswarm-option-group', 'payswarm_default_auth_rate');
   register_setting('payswarm-option-group', 'payswarm_default_currency');
   register_setting('payswarm-option-group', 'payswarm_default_price');
   register_setting('payswarm-option-group', 'payswarm_destination_account');
   register_setting('payswarm-option-group', 'payswarm_default_license_url');
   register_setting('payswarm-option-group', 'payswarm_default_license_hash');
   register_setting('payswarm-option-group', 'payswarm_contracts_url');
   register_setting('payswarm-option-group', 'payswarm_public_key');
   register_setting('payswarm-option-group', 'payswarm_private_key');
   register_setting('payswarm-option-group', 'payswarm_key_overwrite');
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
      if(payswarm_config_update(array('payswarm_config_url')))
      {
         // Get the PaySwarm Authority config information
         $config_url = get_option('payswarm_config_url');

         // FIXME: Ignore SSL - we will want to enable SSL verification
         $response = wp_remote_get($config_url, array('sslverify' => false));

         if(!is_wp_error($response) && 
            payswarm_config_endpoints($response["body"]))
         {
            wp_redirect(plugins_url() . '/payswarm/payswarm-register.php');
         }
         else
         {
            payswarm_admin_warning(
               'There was an error reading the PaySwarm Authority ' .
               'Configuration. This may have happened because the ' .
               'PaySwarm Authority configuration specified was incorrect or ' .
               'because the PaySwarm Authority you listed is down.');
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
   $options_not_being_set = !isset($_POST['payswarm_register_button']) ||
      !isset($_POST['payswarm_update_button']);
   $options_nonexistant = !get_option('payswarm_config_url') ||
      !get_option('payswarm_default_auth_rate') ||
      !get_option('payswarm_default_currency') ||
      !get_option('payswarm_default_price') ||
      !get_option('payswarm_destination_account') ||
      !get_option('payswarm_default_license_url') ||
      !get_option('payswarm_default_license_hash') ||
      !get_option('payswarm_contracts_url') ||
      !get_option('payswarm_public_key') ||
      !get_option('payswarm_private_key');
   $options_invalid = strlen(get_option('payswarm_config_url')) < 1 ||
      strlen(get_option('payswarm_default_auth_rate')) < 1 ||
      strlen(get_option('payswarm_default_currency')) < 1 ||
      strlen(get_option('payswarm_default_price')) < 1 ||
      strlen(get_option('payswarm_destination_account')) < 1 ||
      strlen(get_option('payswarm_default_license_url')) < 1 ||
      strlen(get_option('payswarm_default_license_hash')) < 1 ||
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
 * Displays the PaySwarm settings page to the administrator.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_settings_page()
{
   if(isset($_POST['payswarm_update_button']))
   {
      // FIXME: this code does not validate input or ensure amounts use 7
      // decimal places, etc.
      check_admin_referer('payswarm-save-config', 'payswarm-nonce');
      payswarm_config_update(array(
         'payswarm_config_url',
         'payswarm_keys_url',
         'payswarm_licenses_url',
         'payswarm_contracts_url',
         'payswarm_public_key',
         'payswarm_private_key',
         'payswarm_default_auth_rate',
         'payswarm_default_currency',
         'payswarm_default_price',
         'payswarm_destination_account',
         'payswarm_default_license_url',
         'payswarm_default_license_hash'));
      // special case checkboxes
      if(isset($_POST['payswarm_key_overwrite']))
      {
         update_option('payswarm_key_overwrite', 'true');
      }
      else
      {
         delete_option('payswarm_key_overwrite');
      }
   }

?>
<div class="wrap">
<h2>PaySwarm Configuration</h2>

<p>
PaySwarm enables you to sell your articles, photos and other content directly
to your website visitors without requiring them to fill in any of their credit
card or banking details. You can charge as little as a penny per article,
or up to hundreds of dollars for your content.
To use PaySwarm, you must first create an account on a 
<a href="http://dev.payswarm.com/">PaySwarm Authority</a>.
</p>

<form method="post" action="">
    <?php settings_fields('payswarm-settings-group'); ?>
    <table class="form-table">

        <tr valign="top">
          <td scope="row" colspan="2">
            <h3><?php _e('Basic PaySwarm Settings') ?></h3>
            <p>After you have created an account on a 
              <a href="http://dev.payswarm.com/">PaySwarm Authority</a>, 
              enter the website for the PaySwarm Authority below. For example:
              <code>http://dev.payswarm.com/</code>.
            </p>
          </td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Authority') ?></th>
        <td><input type="text" size="64"
          id="payswarm_authority" name="payswarm_authority"
          onkeyup="javascript:updateAuthorityConfigUrl();"
          value="dev.payswarm.com" />
           <p>Configuration: 
              <span id="payswarm_config_url_display">
                https://dev.payswarm.com/payswarm-v1-config</span>
              <input type="hidden" id="payswarm_config_url" 
                name="payswarm_config_url" 
                value="https://dev.payswarm.com/payswarm-v1-config" />
           </p>
        </td>
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
        <th scope="row"><?php _e('Registration Service') ?></th>
        <td><input type="text" size="128"
          id="payswarm_registration_url" name="payswarm_registration_url"
          value="<?php echo get_option('payswarm_registration_url'); ?>" /></td>
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
        <th scope="row"><?php _e('Public Key') ?></th>
        <td><textarea cols="80" rows="6"
          id="payswarm_public_key" name="payswarm_public_key"
          ><?php echo get_option('payswarm_public_key'); ?></textarea></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Private Key') ?></th>
        <td><textarea cols="80" rows="6"
          id="payswarm_private_key" name="payswarm_private_key"
          ><?php echo get_option('payswarm_private_key'); ?></textarea></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('Overwrite Key Pair') ?></th>
        <td><input type="checkbox"
          id="payswarm_key_overwrite" name="payswarm_key_overwrite"
          value="true"
          <?php if(get_option('payswarm_key_overwrite')) echo 'checked="checked"'; ?>"/></td>
        </tr>

        <tr valign="top">
        <td scope="row" colspan="2">
          <h3><?php _e('Financial Account and License Defaults') ?></h3>
          <p>The financial account information below is used to specify
          default accounts, prices and rates used when selling content from
          this site. The default price is the amount of money that is charged
          per article by default. The default Authority rate is the maximum
          percentage a PaySwarm Authority can take in processing fees.
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
        <th scope="row"><?php _e('Default Price') ?></th>
        <td>
          <input type="text" size="8" name="payswarm_default_price"
            value="<?php echo get_option('payswarm_default_price'); ?>" /> USD
        </td>

        <tr valign="top">
        <th scope="row"><?php _e('Default Authority Rate') ?></th>
        <td>
          <input type="text" size="8" name="payswarm_default_auth_rate"
            value="<?php echo get_option('payswarm_default_auth_rate'); ?>" /> %
        </td>

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
           echo '<p>Default License Hash: ' .
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
