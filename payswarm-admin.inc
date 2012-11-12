<?php

require_once('payswarm-wp-hooks.inc');

/**
 * Called when a user accesses the admin area.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_admin_init() {
  // set up pages
  payswarm_setup_register_page();
  payswarm_setup_options_page();
}

/**
 * Add a PaySwarm configuration link to the admin panel.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_admin_menu() {
  if(!current_user_can('manage_options')) {
    return;
  }

  // add registration page
  add_submenu_page(
    'plugins.php', 'PaySwarm', 'PaySwarm',
    'administrator', 'payswarm', 'payswarm_render_register_page');

  // add settings page
  add_options_page(
    'PaySwarm Settings', 'PaySwarm',
    'manage_options', 'payswarm', 'payswarm_render_options_page');
}

/**
 * Gets the PaySwarm options.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return the payswarm options.
 */
function payswarm_get_options() {
  // get default payswarm options
  return get_option('payswarm_options', array(
    'default_price' => '0.05',
    'default_license_id' => 'http://purl.org/payswarm/licenses/blogging',
    'default_license_hash' =>
      'urn:sha256:' .
      '068663468cfa0c2559c3e7cbaf685077ac4b8271978a64f77c74d502829f7472',
    'default_authority_rate' => '10.0'
  ));
}

/**
 * Sets up the PaySwarm options page.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_setup_options_page() {
  // get payswarm options
  $options = payswarm_get_options();

  // add payswarm option section
  add_settings_section(
    'payswarm_options_section',
    'PaySwarm Options',
    'payswarm_render_options_section',
    'payswarm');

  // add payswarm option fields
  add_settings_field(
    'payswarm_default_price',
    'Default Paid Content Price',
    'payswarm_render_default_price',
    'payswarm',
    'payswarm_options_section',
    $options);
  add_settings_field(
    'payswarm_default_license_id',
    'Default Paid Content License',
    'payswarm_render_default_license_id',
    'payswarm',
    'payswarm_options_section',
    $options);
  add_settings_field(
    'payswarm_default_authority_rate',
    'Default Authority Rate',
    'payswarm_render_default_authority_rate',
    'payswarm',
    'payswarm_options_section',
    $options);

  // register payswarm options
  register_setting(
    'payswarm',
    'payswarm_options',
    'payswarm_validate_options');
}

/**
 * Sets up the PaySwarm registration page.
 */
function payswarm_setup_register_page() {
  // add payswarm registration section
  add_settings_section(
    'payswarm_register_section',
    'PaySwarm Registration',
    'payswarm_render_register_section',
    'payswarm_register');

  // add payswarm register fields
  add_settings_field(
    'payswarm_authority_host',
    'PaySwarm Authority',
    'payswarm_render_authority_host',
    'payswarm_register',
    'payswarm_register_section');
  add_settings_field(
    'payswarm_overwrite_key_pair',
    'Overwrite Existing Key Pair',
    'payswarm_render_overwrite_key_pair',
    'payswarm_register',
    'payswarm_register_section');

  // no need to register settings, options.php is not used for registration
}

/**
 * Initiates vendor registration with a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_init_registration() {
  // do nonce security check (dies on failure)
  $valid_nonce = isset($_POST['_wpnonce']) && wp_verify_nonce(
    $_POST['_wpnonce'], 'payswarm_register-options');
  if(!$valid_nonce) {
    wp_die(__('Access denied.'));
  }

  try {
     // validate authority host
     if(!isset($_POST['payswarm_authority_host']) or
       $_POST['payswarm_authority_host'] === '') {
       throw new Exception('Invalid PaySwarm Authority host.');
     }
     $host = $_POST['payswarm_authority_host'];
     update_option('payswarm_authority_host', $host);

     // sanitize overwrite key pair
     $overwrite = isset($_POST['payswarm_overwrite_key_pair']);
     update_option('payswarm_overwrite_key_pair', $overwrite);

     // get register vendor url
     $callback = plugins_url() . '/payswarm/payswarm-register.php';
     $url = payswarm_get_register_vendor_url($host, $callback, $overwrite);

     // redirect to the PaySwarm Authority registration URL
     header('HTTP/1.1 303 See Other');
     header("Location: $url");
  }
  catch(Exception $e) {
    // close the popup and report an error
    $url = admin_url() . 'plugins.php?page=payswarm&error=config';
    payswarm_close_register_popup($url);
  }
}

/**
 * Completes vendor registration with a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $response the JSON encoded encrypted registration response.
 */
function payswarm_complete_registration($response) {
  try {
    // get vendor preferences from response
    $prefs = payswarm_register_vendor($response);

    // save vendor information, set registered
    update_option('payswarm_vendor_id', $prefs->owner);
    update_option('payswarm_financial_account', $prefs->destination);
    update_option('payswarm_registered', 1);

    // trigger resign of listings
    payswarm_trigger_listing_resign();

    // close popup and show admin page
    $url = admin_url() . 'plugins.php?page=payswarm&complete=true';
    payswarm_close_register_popup($url);
  }
  catch(PaySwarmAsymmetricCipherException $e) {
    // close the popup and report decryption error
    $url = admin_url() . 'plugins.php?page=payswarm&error=decryption';
    payswarm_close_register_popup($url);
  }
  catch(Exception $e) {
    // close the popup and report an error
    $url = admin_url() . 'plugins.php?page=payswarm&error=authority';
    payswarm_close_register_popup($url);
  }
  exit(0);
}

/**
 * A helper function that outputs JavaScript to close the registration
 * popup.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $url the URL to load in the parent window.
 */
function payswarm_close_register_popup($url) {
  $script = PAYSWARM_PLUGIN_URL . 'payswarm.js';
  ?>
  <html><body>
  <script type="text/javascript" src="<?php echo $script; ?>"></script>
  <script type="text/javascript">closePopup('<?php echo $url; ?>');</script>
  </body></html>
  <?php
}

/**
 * Adds a warning to the administration page.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $warning The warning to print to the administration page.
 */
function payswarm_admin_warning($warning) {
  ?>
  <div id='payswarm-admin-warning' class='updated fade'>
    <p>
      <?php _e($warning); ?>
    </p>
    <p>
      You still need to
      <a href="plugins.php?page=payswarm">configure PaySwarm</a>.
    </p>
  </div>
  <?php
}

/**
 * Displays the PaySwarm registration page to the administrator.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_register_page() {
  $register_url = PAYSWARM_PLUGIN_URL . 'payswarm-register.php';
  ?>
  <div class="wrap">
    <?php screen_icon('options-general'); ?>
    <h2>PaySwarm</h2>
    <form action="<?php echo $register_url; ?>" method="post"
      onsubmit="javascript:showAuthorityPopup(this);">
      <?php settings_fields('payswarm_register'); ?>
      <?php do_settings_sections('payswarm_register'); ?>
      <p class="submit">
        <input name="submit" type="submit" class="button-primary"
          value="<?php _e('Register this site'); ?>" />
      </p>
    </form>
    <p>To manage your PaySwarm options after registering go to the
      <a href="options-general.php?page=payswarm">PaySwarm Options</a> page.
    </p>
  </div>
  <?php

  // output warnings
  if(isset($_GET['error'])) {
    $host = get_option('payswarm_authority_host');
    $url = "https://$host/.well-known/payswarm";
    if($_GET['error'] === 'config') {
      payswarm_admin_warning(
        'There was an error fetching the PaySwarm Authority ' .
        'Configuration (' . htmlentities($url) . '). ' .
        'This may have happened because the ' .
        'PaySwarm Authority configuration URL specified was incorrect ' .
        'or because the PaySwarm Authority you specified is not ' .
        'responding.');
    }
    else if($_GET['error'] === 'authority') {
      payswarm_admin_warning(
        'The PaySwarm Authority reported an error during registration. ' .
        'Please try again.');
    }
    else if($_GET['error'] === 'decryption') {
      payswarm_admin_warning(
        'The response from the PaySwarm Authority could not be decrypted. ' .
        'If this website is not protected by SSL-encryption, ' .
        'then there may be a serious security problem. If this site does ' .
        'not use "https" in the URL bar or does not appear to be a secure ' .
        'site in your browser, then please please log into your PaySwarm ' .
        'Authority\'s website and disable any "Access Keys" or "Public Keys" ' .
        'associated with this website before re-attempting registration. ' .
        'If this problem persists, please contact your PaySwarm Authority ' .
        'for assistance.');
    }
  }
  else if($_GET['complete'] === 'true') {
    $register_url = PAYSWARM_PLUGIN_URL . 'payswarm-register.php';
    ?>
    <div id='payswarm-admin-warning' class='updated fade'>
      <p>
        Registration successful. Please proceed to the <a href="options-general.php?page=payswarm">PaySwarm Options</a> page.
      </p>
    </div>
    <?php
  }
}

/**
 * Renders the description for the PaySwarm register section.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_register_section() {
  // show settings errors, if any
  settings_errors();
  ?>
  <p>PaySwarm enables you to sell your articles, photos, and other content
    directly to your website visitors without requiring them to create accounts
    or fill in any of their credit card or banking details. You can charge as
    little as a penny per article or up to hundreds of dollars for your content.
    To use PaySwarm, you must first sign up with a
    <a href="<?php echo PAYSWARM_AUTHORITY_URL; ?>">PaySwarm Authority</a>.
  </p>
  </p>After you have created an account on a
    <a href="<?php echo PAYSWARM_AUTHORITY_URL; ?>">PaySwarm Authority</a>,
    enter the website for the PaySwarm Authority below. For example:
    <code><?php echo PAYSWARM_AUTHORITY_HOST; ?></code>.
  </p>
  <?php
}

/**
 * Renders the PaySwarm Authority host field.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_authority_host() {
  $value = get_option('payswarm_authority_host', '');
  ?>
  <input id="payswarm_authority_host" type="text" size="32"
    name="payswarm_authority_host"
    value="<?php echo $value; ?>" />
  <label for="payswarm_authority_host">eg: dev.payswarm.com</label>
  <?php
}

/**
 * Renders the overwrite key pair field.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_overwrite_key_pair() {
  $value = get_option('payswarm_overwrite_key_pair', '');
  ?>
  <input id="payswarm_overwrite_key_pair" type="checkbox"
    name="payswarm_overwrite_key_pair"
    value="1" <?php checked($value); ?> />
  <label for="payswarm_overwrite_key_pair">
    Check to always create a new key pair before registering
    (default: off)</label>
  <?php
}

/**
 * Displays the PaySwarm options page to the administrator.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_options_page() {
  // show registration page if not registered with an authority yet
  if(!get_option('payswarm_registered', false)) {
    payswarm_render_register_page();
  }
  else {
     ?>
     <div class="wrap">
       <?php screen_icon('options-general'); ?>
         <h2>PaySwarm</h2>
         <form action="options.php" method="post">
           <?php settings_fields('payswarm'); ?>
           <?php do_settings_sections('payswarm'); ?>
           <p class="submit">
             <input name="submit" type="submit" class="button-primary"
               value="<?php _e('Save Changes'); ?>" />
           </p>
         </form>
       </div>
     <?php
  }
}

/**
 * Renders the description for the PaySwarm options section.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_options_section() {
  // show settings errors, if any
  settings_errors();
  ?>
  <p>This page allows you to configure the default settings used for content
    your customers must pay for to access. The PaySwarm plugin allows you
    specify entire posts or just parts of posts that require payment. Below
    are default settings that only apply to content that you specify must be
    paid for. You can use settings other than these defaults when editing
    individual posts, but you may often want to reuse the same settings, so
    these defaults save you time.</p>
  <?php
}

/**
 * Renders the default price field.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $options the payswarm options.
 */
function payswarm_render_default_price($options) {
  ?>
  <input type="text" size="8" name="payswarm_options[default_price]"
    value="<?php echo $options['default_price']; ?>" /> USD
  <p>This is the default price that will be used for any content
    in a post that you specify must be paid for.</p>
  <?php
}

/**
 * Renders the default license ID field.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $options the payswarm options.
 */
function payswarm_render_default_license_id($options) {
  $hash = $options['default_license_hash'];
  if($hash === '') {
    $hash = 'Save settings to compute';
  }
  ?>
  <input type="text" size="64" name="payswarm_options[default_license_id]"
    value="<?php echo $options['default_license_id']; ?>" />
  <p>Hash: <?php echo $hash ?></p>
  <p>This is the URL for the default license to use for any
    content in a post that you specify must be paid for.</p>
  <?php
}

/**
 * Renders the default authority rate field.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $options the payswarm options.
 */
function payswarm_render_default_authority_rate($options) {
  ?>
  <input type="text" size="8" name="payswarm_options[default_authority_rate]"
    value="<?php echo $options['default_authority_rate']; ?>" />
  <p>This is the percentage of the price paid by your customers
    for content that you permit a PaySwarm Authority to collect in
    exchange for processing payments. If it is too low, a PaySwarm
    Authority may elect to refuse your business.</p>
  <?php
}

/**
 * Validates payswarm options input.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param array $input the submitted payswarm options.
 *
 * @return array the validated payswarm options.
 */
function payswarm_validate_options($input) {
  $options = payswarm_get_options();

  // validate default price
  if(payswarm_is_decimal($input['default_price']) &&
    floatval($input['default_price']) >= 0) {
    $options['default_price'] = sprintf('%0.7f', $input['default_price']);
  }
  else {
    add_settings_error(
      'payswarm_options',
      'payswarm_default_price_error',
      __('The default price must be a number greater than or equal to 0 and
      have the format: x.xx.'));
  }

  // validate default license
  try {
    $authority_host = get_option('payswarm_authority_host');
    $license_id = $options['default_license_id'];
    if(wp_kses_bad_protocol($license_id, array('http', 'https')) !==
      $license_id) {
      throw new Exception('Invalid license ID.');
    }

    // get license
    $license = payswarm_authority_cache_license($authority_host, $license_id);
    $options['default_license_id'] = $license_id;
    $options['default_license_hash'] = payswarm_hash($license);
  }
  catch(Exception $e) {
    add_settings_error(
      'payswarm_options',
      'payswarm_default_license_id_error',
      __('The default license must be a valid URL that contains a ' .
      'PaySwarm license.'));
  }

  // validate default authority rate
  if(payswarm_is_decimal($input['default_authority_rate']) &&
    floatval($input['default_authority_rate']) >= 0 &&
    floatval($input['default_authority_rate']) <= 100) {
    $options['default_authority_rate'] = sprintf(
      '%0.7f', $input['default_authority_rate']);
  }
  else {
    add_settings_error(
      'payswarm_options',
      'payswarm_default_authority_rate_error',
      __('The default authority rate must be a number between 0 and 100.'));
  }

  // trigger listing resign
  payswarm_trigger_listing_resign();

  return $options;
}

/**
 * Returns true if the given input is of the given decimal format.
 *
 * @param string @input the input.
 *
 * @return true if the format is valid, false if not.
 */
function payswarm_is_decimal($input, $format='%0.7f') {
  return sprintf($format, $input) == $input;
}

/**
 * Sets a flag that indicates that listings need to be resigned due to
 * registration or options changes.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_trigger_listing_resign() {
  // set resign_if_before date
  $now = date_format(date_create(), DateTime::W3C);
  update_option('payswarm_resign_if_before', $now);
}

/* end of file, omit ?> */
