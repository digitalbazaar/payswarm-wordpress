<?php

$options_not_being_set = !isset($_POST['payswarm_consumer_key']) ||
   !isset($_POST['payswarm_consumer_secret']) ||
   !isset($_POST['payswarm_default_currency']);

if(is_admin())
{
   // admin actions
   add_action('admin_menu', 'payswarm_create_menu');
   add_action('admin_init', 'payswarm_register_settings');

   // create a warning if the proper settings are not being set.
   if($options_not_being_set)
   {
      payswarm_admin_warnings();
   }
}

function payswarm_register_settings()
{
   // whitelist options
   register_setting('payswarm-option-group', 'payswarm_consumer_key');
   register_setting('payswarm-option-group', 'payswarm_consumer_secret');
   register_setting('payswarm-option-group', 'payswarm_default_currency');
}

function payswarm_create_menu()
{
   //create new top-level menu
   add_submenu_page('plugins.php', 'PaySwarm Configuration', 'PaySwarm', 
      'administrator', 'payswarm', 'payswarm_settings_page');
}

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

function payswarm_admin_warnings() 
{
   global $options_not_being_set;
   $options_nonexistant = !get_option('payswarm_consumer_key') ||
      !get_option('payswarm_consumer_secret') ||
      !get_option('payswarm_default_currency');
   $options_invalid = strlen(get_option('payswarm_consumer_key')) < 1 ||
      strlen(get_option('payswarm_consumer_secret')) < 1 ||
      strlen(get_option('payswarm_default_currency')) < 1;

   if(($options_nonexistant && $options_not_being_set) || $options_invalid)
   {
      function psaw_config()
      {
         payswarm_admin_warning(
            'The PaySwarm Client ID and Client Secret are not set.');
      }
      add_action('admin_notices', 'psaw_config');
   }
}

function payswarm_settings_page()
{
   // update the consumer key, secret and default currency
   if(isset($_POST['payswarm_consumer_key']) &&
      isset($_POST['payswarm_consumer_secret']) &&
      isset($_POST['payswarm_default_currency']))
   {
      check_admin_referer('payswarm-save-config', 'payswarm-nonce');
      $consumer_key = $_POST['payswarm_consumer_key'];
      $consumer_secret = $_POST['payswarm_consumer_secret'];
      $default_currency = $_POST['payswarm_default_currency'];

      update_option('payswarm_consumer_key', $consumer_key);
      update_option('payswarm_consumer_secret', $consumer_secret);
      update_option('payswarm_default_currency', $default_currency);
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
<strong>PaySwarm Consumer Key</strong> and a
<strong>PaySwarm Consumer Secret</strong>, which you must enter below to
activate PaySwarm on your site.
</p>

<form method="post" action="">
    <?php settings_fields( 'payswarm-settings-group' ); ?>
    <table class="form-table">
        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Client ID') ?></th>
        <td><input type="text" size=64 name="payswarm_consumer_key" 
          value="<?php echo get_option('payswarm_consumer_key'); ?>" /></td>
        </tr>

        <tr valign="top">
        <th scope="row"><?php _e('PaySwarm Client Secret') ?></th>
        <td><input type="text" size=64 name="payswarm_consumer_secret" 
          value="<?php echo get_option('payswarm_consumer_secret'); ?>" /></td>

        <tr valign="top">
        <th scope="row"><?php _e('Default Currency') ?></th>
        <td>
          <select name="payswarm_default_currency">
            <option value="USD">$ - Dollars (USD)</option>
          </select>
        </td>
        </tr>
    </table>

    <?php wp_nonce_field('payswarm-save-config', 'payswarm-nonce') ?>

    <p class="submit">
    <input type="submit" class="button-primary" 
      value="<?php _e('Save Changes') ?>" />
    </p>

</form>
</div>
<?php if(!empty($_POST['payswarm_consumer_key'])) : ?>
<div id="message" class="updated fade"><p><strong><?php _e('PaySwarm options saved.') ?></strong></p></div>
<?php endif; ?>
<?php 
}
?>
