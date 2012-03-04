<?php

require_once('payswarm-wp-hooks.inc');
require_once('payswarm-database.inc');
require_once('payswarm-session.inc');
require_once('payswarm-admin.inc');

/**
 * Initiates an article purchase with a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_init_purchase() {
  // get post to access
  $post_id = $_GET['p'];

  // get customer's PaySwarm Authority host
  $host = payswarm_get_authority_host_cookie();
  if($host === false) {
    // TODO: UI to select the PaySwarm Authority if one isn't already selected
    // get authority host from a UI
    //$host =
    // FIXME: use the authority for the customer, not the vendor
    $host = get_option('payswarm_authority_host');
    payswarm_set_authority_host_cookie($host);
  }

  // get payment url
  $info = payswarm_get_post_info($post_id);
  $callback = plugins_url() . '/payswarm/payswarm-access.php?p=' . $post_id;
  $url = payswarm_get_purchase_url(
    $host, $info['listing']->{'@id'}, $info['listing_hash'], $callback);

  // redirect to the PaySwarm Authority payment URL
  header('HTTP/1.1 303 See Other');
  header("Location: $url");
}

/**
 * Completes an article purchase with a PaySwarm Authority.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $response the JSON encoded encrypted purchase response.
 */
function payswarm_complete_purchase($response) {
  echo "<html><body>";
  try {
    // get receipt from response
    $receipt = payswarm_register_vendor($response);

    // get contract info
    $identity_id = $receipt->{'ps:assetAcquirer'};
    $asset = $receipt->{'ps:asset'};
    $license = $receipt->{'ps:license'};

    // get post ID from asset URL (strip after hash)
    $post_id = url_to_postid(preg_replace('/#.*/', '', $asset));
    if($post_id === 0) {
      throw new Exception('PaySwarm Purchase Exception: ' .
        'The Asset in the Contract could not be matched to a post.');
    }

    // create/update payswarm session
    $session = payswarm_create_session($identity_id);

    // authorize the post
    if(!payswarm_database_authorize_post($identity_id, $post_id, $license)) {
      throw new Exception('PaySwarm Purchase Exception: ' .
        'A record of the purchase could not be written to the database.');
    }

    // close popup and show post
    $url = get_permalink($post_id);
    payswarm_close_purchase_popup($url);
  }
  catch(Exception $e) {
    // close the popup and return to the post
    // FIXME: report the error
    $url = get_permalink($_GET['p']);
    payswarm_close_purchase_popup($url);
  }
  echo "</body></html>";
  exit(0);
}

/**
 * A helper function that outputs JavaScript to close the purchase
 * popup.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $url the URL to load in the parent window.
 */
function payswarm_close_purchase_popup($url) {
  $script = PAYSWARM_PLUGIN_URL . 'payswarm.js';
  ?>
  <script type="text/javascript" src="<?php echo $script; ?>"></script>
  <script type="text/javascript">closePopup('<?php echo $url; ?>');</script>
  <?php
}

/**
 * Determines whether or not the given Asset is stale. An Asset is considered
 * stale if it isn't a valid Asset or if its signature was created before the
 * global resign date flag.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $asset the Asset to check.
 *
 * @return boolean true if the Asset is stale, false if not.
 */
function payswarm_is_asset_stale($asset) {
  // no asset
  if($asset === '') {
    return true;
  }

  // no resign date set yet
  $resign = get_option('payswarm_resign_if_before', true);
  if($resign === true) {
    // trigger listing resign
    payswarm_trigger_listing_resign();
    return true;
  }

  // compare dates
  $resign = date_create($resign);
  $created = date_create($asset->{'sec:signature'}->{'dc:created'});
  if($created <= $resign) {
    return true;
  }

  return false;
}

/**
 * Returns an associative array of the PaySwarm related information for a post.
 * If the information is determined to be stale, it will be automatically
 * regenerated and updated in the database.
 *
 * The info will include:
 * 'asset': The signed Asset for the post.
 * 'listing': The signed Listing for the post.
 * 'listing_hash': The Listing hash.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post.
 *
 * @return array an associative array with PaySwarm related information.
 */
function payswarm_get_post_info($post_id) {
  // get asset
  $asset = get_post_meta($post_id, '_payswarm_asset', true);

  // if asset is stale, force update
  $update = payswarm_is_asset_stale($asset);
  if(!$update) {
    // get listing
    $listing = get_post_meta($post_id, '_payswarm_listing', true);

    // if listing is not valid, force update
    $update = !payswarm_is_listing_valid($listing);
    if(!$update) {
      // get listing hash
      $listing_hash = get_post_meta($post_id, '_payswarm_listing_hash', true);
    }
  }

  if($update) {
    $rval = payswarm_update_post_info($post_id);
  }
  else {
    $rval = array(
      'asset' => $asset,
      'listing' => $listing,
      'listing_hash' => $listing_hash);
  }

  return $rval;
}

/**
 * Updates the PaySwarm information related to a post and returns it.
 *
 * The info will include:
 * 'asset': The signed Asset for the post.
 * 'listing': The signed Listing for the post.
 * 'listing_hash': The Listing hash.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post.
 *
 * @return array an associative array with PaySwarm related information.
 */
function payswarm_update_post_info($post_id) {
  // get post information
  $post = get_post($post_id);
  $post_title = $post->post_title;
  $post_author = get_userdata($post->post_author)->display_name;

  // normalize post URL to http so two different assets aren't produced
  // based on the different protocol
  $post_url = preg_replace('/^https/', 'http', get_permalink($post_id));

  // get payswarm options
  $options = get_option('payswarm_options');

  // create asset
  $asset = (object)array(
    '@context' => payswarm_create_default_jsonld_context(),
    '@id' => $post_url . '#asset',
    '@type' => array('ps:Asset', 'ps:WebPage'),
    'dc:creator' => (object)array('foaf:name' => $post_author),
    'dc:title' => $post_title,
    'ps:assetProvider' => get_option('payswarm_vendor_id'),
    'ps:contentUrl' => $post_url
  );

  // sign asset
  payswarm_sign($asset);

  // generate new listing validity period at start of the day
  $date = date_create('now', new DateTimeZone('UTC'));
  $next24 = clone $date;
  date_add($next24, date_interval_create_from_date_string('1 day'));
  $valid_from = date_format($date, DateTime::W3C);
  $valid_until = date_format($next24, DateTime::W3C);

  // get license ID and hash
  $license_id = false;
  $license_hash = false;
  $use_default_license = get_post_meta(
    $post_id, '_payswarm_use_default_license', true);
  if($use_default_license === 0) {
    $license_id = get_post_meta($post_id, '_payswarm_license_id', true);
    $license_hash = get_post_meta($post_id, '_payswarm_license_hash', true);
  }
  if(!$license_id or !$license_hash) {
    $license_id = $options['default_license_id'];
    $license_hash = $options['default_license_hash'];
  }

  // create listing
  $listing = (object)array(
    '@context' => payswarm_create_default_jsonld_context(),
    '@id' => $post_url . '#listing',
    '@type' => array('ps:Listing', 'gr:Offering'),
    'com:payee' => array(
      (object)array(
        '@id' => $post_url . '#listing-payee',
        '@type' => 'com:Payee',
        'com:payeePosition' => 0,
        'com:destination' => get_option('payswarm_financial_account'),
        'com:rate' => get_post_meta($post_id, '_payswarm_price', true),
        'com:rateType' => 'com:FlatAmount',
        'rdfs:comment' => "Payment for $post_title by $post_author."
      )
    ),
    'com:payeeRule' => array(
      (object)array(
        '@type' => 'com:PayeeRule',
        'com:destinationOwnerType' => 'ps:Authority',
        'com:maximumRate' => get_post_meta(
          $post_id, '_payswarm_authority_rate', true),
        'com:rateContext' => array(
          'com:Inclusive',
          'com:Tax',
          'com:TaxExempt'),
        'com:rateType' => 'com:Percentage'
      )
    ),
    'ps:asset' => $asset->{'@id'},
    'ps:assetHash' => payswarm_hash($asset),
    'ps:license' => $license_id,
    'ps:licenseHash' => $license_hash,
    'ps:validFrom' => $valid_from,
    'ps:validUntil' => $valid_until
  );

  // sign listing
  payswarm_sign($listing);

  // hash listing
  $listing_hash = payswarm_hash($listing);

  // update post meta
  update_post_meta($post_id, '_payswarm_asset', $asset);
  update_post_meta($post_id, '_payswarm_listing', $listing);
  update_post_meta($post_id, '_payswarm_listing_hash', $listing_hash);

  return array(
    'asset' => $asset,
    'listing' => $listing,
    'listing_hash' => $listing_hash);
}

/**
 * Generates the PaySwarm post information HTML+RDFa markup, which can be
 * inserted into parts of the WordPress site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post that is being expressed.
 *
 * @return string the HTML markup, surrounded by a DIV element, that contains
 *         the necessary RDFa markup to express the PaySwarm information.
 */
function payswarm_get_post_info_rdfa($post_id) {
  // get payswarm post info
  $info = payswarm_get_post_info($post_id);

  // get URLs (add post ID to access URL as query param)
  $access_url = PAYSWARM_PLUGIN_URL . 'payswarm-access.php';
  $access_url = add_query_arg('p', $post_id, $access_url);
  $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';

  // set vars for template
  $payee = $info['listing']->{'com:payee'}[0];
  $payee_rule = $info['listing']->{'com:payeeRule'}[0];
  $vars = array(
    'asset' => $info['asset'],
    'listing' => $info['listing'],
    'listing_hash' => $info['listing_hash'],
    'asset_types' => join('', $info['asset']->{'@type'}),
    'listing_types' => join('', $info['listing']->{'@type'}),
    'access_url' => $access_url,
    'pslogo_url' => $pslogo_url,
    'payee' => $payee,
    'payee_rate' => sprintf('%1.02f', $payee->{'com:rate'}),
    'payee_comment' => htmlentities($payee->{'rdfs:comment'}),
    'payee_rule' => $payee_rule,
    'post_title' => htmlentities($info['asset']->{'dc:title'}),
    'post_author' =>
      htmlentities($info['asset']->{'dc:creator'}->{'foaf:name'}),
    'text_access' => __('Access'),
    'text_view_full_article' => __('View the full article for'),
    'text_view_offer_details' => __('View offer details'),
    'output' => ''
  );

  // generate template output
  include('payswarm-rdfa.inc');
  return $vars['output'];
}

/**
 * Filters the paid content for posts.
 *
 * This filter is applied based the current PaySwarm session. If the visitor
 * has not been granted access to an article, only the free content is
 * displayed. If the visitor has paid for the content then the paid content
 * is shown. This is a filter that is applied to the 'the_content' WordPress
 * filter.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $content the article content as it is stored in the database.
 *
 * @return string the article content after modification by this filter.
 */
function payswarm_filter_paid_content($content) {
  global $post;

  // check to see if the article has been purchased already
  $purchased = false;
  $post_id = $post->ID;
  $session = payswarm_get_session();
  $purchased = payswarm_database_is_post_authorized($session, $post_id);

  // FIXME: include these tags in the regex below to optimize ... or maybe
  // store different post versions as metadata on article save and load
  // that here (pick a way to reduce dynamic loading overhead)

  // remove any spurious paragraph and break tags
  $content = str_replace(
    '<p><!--payswarm--></p>',
    '<!--payswarm-->', $content);
  $content = str_replace(
    '<p><!--payswarm--><br />',
    '<!--payswarm-->', $content);
  $content = str_replace(
    '<p><!--payswarm-alternate--></p>',
    '<!--payswarm-alternate-->', $content);
  $content = str_replace(
    '<p><!--payswarm-alternate--><br />',
    '<!--payswarm-alternate-->', $content);

  // if the article has not been purchased and there is for-pay content
  if(!$purchased && preg_match('/<!--payswarm-->.+/ms', $content)) {
    // remove all for-pay content
    $processed_content = payswarm_filter_content($content, true);

    // if a price is available, add purchase UI
    $amount = get_post_meta($post_id, '_payswarm_price', true);
    if($amount !== '') {
      $processed_content .=
        '<div class="purchase section">' .
        '<div class="money row">' .
        payswarm_get_post_info_rdfa($post_id) .
        '</div></div>';
    }
    // no purchase price available
    else {
      // tell the visitor that the article cannot be purchased
      $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';
      $processed_content .=
        '<div class="purchase section">' .
        '<div class="money row">' .
        '<span class="label">' .
          __('The author has not set the price for the paid content.') .
        '</span><button class="purchase-button">' .
        '<img alt="' . __('Cannot Purchase') .'" src="' . $pslogo_url .
        '" />' . __('Cannot Purchase') . '</button>' .
        '</div></div>';
    }
  }
  // article has been purchased or there is no for-pay content
  else {
    // remove alternate content that only displays when visitor hasn't paid
    $processed_content = payswarm_filter_content($content, false);
  }

  return $processed_content;
}

/**
 * A helper function called from payswarm_filter_paid_content that removes
 * either paid or alternate text from an article (based on a flag).
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $content the article content to filter.
 * @param string $paid true to filter out paid content, false to filter out
 *          alternate text.
 *
 * @return string the filtered article.
 */
function payswarm_filter_content($content, $paid) {
  $rval = '';

  // setup start and end tags for filtering
  $start = $paid ? '<!--payswarm-->' : '<!--payswarm-alternate-->';
  $start_len = strlen($start);
  $end = $paid ? '<!--payswarm-alternate-->' : '<!--payswarm-->';

  $pos = 0;
  do {
    // find start of content to remove
    $tmp = strpos($content, $start, $pos);

    // nothing to filter, add all content
    if($tmp === false) {
      $rval .= substr($content, $pos);
    }
    // filter out content
    else {
      // add all content up to start of content to filter out
       $rval .= substr($content, $pos, ($tmp - $pos));

       // find end of content to filter out
       $tmp = strpos($content, $end, $tmp + $start_len);
    }
    $pos = $tmp;
  }
  while($pos !== false);

  // remove special tags from content
  $rval = str_replace('<!--payswarm-->', '', $rval);
  $rval = str_replace('<!--payswarm-alternate-->', '', $rval);

  return $rval;
}

/**
 * Adds the PaySwarm payment information metabox to a post editor box.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_add_meta_boxes() {
  add_meta_box(
    'payswarm_section_id', __('PaySwarm Options'),
    'payswarm_render_meta_box', 'post', 'side', 'high');
}

/**
 * Renders the HTML for a PaySwarm post metadata editing box.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_render_meta_box() {
  global $post;

  // get post info
  $info = payswarm_get_post_info($post->ID);
  $use_default_license = get_post_meta(
    $post->ID, '_payswarm_use_default_license', true);
  if($use_default_license !== 0) {
    $use_default_license = 1;
  }

  // get payee and payee rule from listing
  $payee = $info['listing']->{'com:payee'}[0];
  $payee_rule = $info['listing']->{'com:payeeRule'}[0];

  // output nonce field (required to prevent forgery attacks)
  $plugin = plugin_basename(__FILE__);
  wp_nonce_field($plugin . '_save_meta', '_payswarm_save_meta_nonce');

  ?>
  <div class="payswarm_meta_box">
    <label for="payswarm_price">
      <?php _e('Price'); echo ' (USD)'; ?>:
    </label>
    <p>
      <input class="right" type="text" id="payswarm_price"
        name="payswarm_price"
        value="<?php echo $payee->{'com:rate'}; ?>" />
      <span>The price for access to non-free content.</span>
    </p>

    <label for="payswarm_authority_rate">
      <?php _e('Authority Percentage'); echo ' (%)'; ?>:
    </label>
    <p>
      <input type="text" id="payswarm_authority_rate"
        name="payswarm_authority_rate"
        value="<?php echo $payee_rule->{'com:maximumRate'}; ?>" />
      <span>The percentage of the price to allow a PaySwarm Authority to
        collect.</span>
    </p>

    <label class="inline" for="payswarm_use_default_license"
      onclick="javascript:toggleLicenseInput(
        'payswarm_license_container', 'payswarm_use_default_license');">
      <?php _e('Use Default License'); ?>:
    </label>
    <input type="checkbox" id="payswarm_use_default_license"
      name="payswarm_use_default_license"
      value="<?php echo $use_default_license; ?>"
      onclick="javascript:toggleLicenseInput(
        'payswarm_license_container', 'payswarm_use_default_license');"
      <?php checked($use_default_license); ?> />

    <div id="payswarm_license_container"
      <?php if($use_default_license) { echo 'class="hidden"'; } ?> >
      <label for="payswarm_license_id">
        <?php _e('License'); ?>:
      </label>
      <p>
        <input type="text" id="payswarm_license_id"
          name="payswarm_license_id" size="44"
          value="<?php echo $info['listing']->{'ps:license'}; ?>" />
        <span>The license to associate with the content when it is sold.</span>
      </p>
    </div>
  </div>
  <?php
}

/**
 * Saves the PaySwarm-specific meta-data that is associated with a post.
 *
 * This method is called whenever a save is done for a post. It saves all of
 * the information that is specified in the PaySwarm meta box for the post.
 *
 * This method may be called multiple times per save. Once for revision save,
 * once for autosave, and once for normal save. This function only does a
 * database update on the normal save.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post.
 *
 * @return integer the ID of the post.
 */
function payswarm_save_post_data($post_id) {
  // determine type of save and check edit permission
  $autosave = wp_is_post_autosave($post_id);
  $revision = wp_is_post_revision($post_id);
  $edit_allowed = current_user_can('edit_post', $post_id);

  // only update on non-auto and non-revision save
  if($autosave or $revision or !$edit_allowed) {
    return $post_id;
  }

  // ensure POST nonce is valid
  $valid_nonce = false;
  if(isset($_POST['_payswarm_save_meta_nonce'])) {
    $plugin = plugin_basename(__FILE__);
    $valid_nonce = wp_verify_nonce(
      $_POST['_payswarm_save_meta_nonce'], $plugin . '_save_meta');
  }

  /* Note: If the nonce is not valid, then use the existing data from the
    post. The nonce will be missing (and thus, invalid) when "Quick Edit"
    or xmlrpc were used to save the post data. */
  if($valid_nonce) {
    $price = $_POST['payswarm_price'];
    $authority_rate = $_POST['payswarm_authority_rate'];
    $use_default_license = isset($_POST['payswarm_use_default_license']) ?
      1 : 0;
    if($use_default_license === 0) {
      $authority_host = get_option('payswarm_authority_host');
      $license_id = $_POST['payswarm_license_id'];
      try {
        $license = payswarm_authority_cache_license(
          $authority_host, $license_id);
        $hash = payswarm_hash($license);
      }
      catch(Exception $e) {
        // invalid license, use existing/default
        $use_default_license = get_post_meta(
          $post_id, '_payswarm_use_default_license', true);
        if($use_default_license === 0) {
          $license_id = get_post_meta($post_id, '_payswarm_license_id', true);
          $hash = get_post_meta($post_id, '_payswarm_license_hash', true);
        }
      }
    }

    // sanitize and update post meta data
    payswarm_update_post_meta(
      $post_id, '_payswarm_price', $price);
    payswarm_update_post_meta(
      $post_id, '_payswarm_authority_rate', $authority_rate);
    payswarm_update_post_meta(
      $post_id, '_payswarm_use_default_license', $use_default_license);
    if($use_default_license === 0) {
      payswarm_update_post_meta($post_id, '_payswarm_license_id', $license_id);
      payswarm_update_post_meta($post_id, '_payswarm_license_hash', $hash);
    }
  }

  // update post info
  payswarm_update_post_info($post_id);

  return $post_id;
}

/**
 * Sanitizes PaySwarm-specific post metadata and writes it to the database.
 *
 * If the given data is invalid, reasonable defaults are used in place of the
 * invalid data provided. Anyone calling this method must ensure that the
 * payswarm_update_post_info() method is called after this method in order to
 * ensure that all PaySwarm-related digital signatures are updated.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post.
 * @param string $meta_key the metadata key to update.
 * @param string $meta_value the metadata value to use for the update.
 */
function payswarm_update_post_meta($post_id, $meta_key, $meta_value) {
  if($meta_key === '_payswarm_price') {
    $price = $meta_value;
    if(!is_numeric($price) or floatval($price) < 0) {
      $options = get_option('payswarm_options');
      update_post_meta($post_id, '_payswarm_price', $options['default_price']);
    }
  }
  else if($meta_key === '_payswarm_authority_rate') {
    $authority_rate = $meta_value;
    if(!is_numeric($authority_rate) or floatval($authority_rate) < 0) {
      $options = get_option('payswarm_options');
      $authority_rate = $options['default_authority_rate'];
      update_post_meta($post_id, '_payswarm_authority_rate', $authority_rate);
    }
  }
  else if($meta_key === '_payswarm_license_id' or
    $meta_key === '_payswarm_license_hash') {
    if($meta_value === '') {
      $options = get_option('payswarm_options');
      $license_id = $options['default_license'];
      $license_hash = $options['default_license_hash'];
      update_post_meta(
        $post_id, '_payswarm_license_id', $license_id);
      update_post_meta(
        $post_id, '_payswarm_license_hash', $license_hash);
    }
  }
  else {
    // update non-sanitized PaySwarm metadata
    update_post_meta($post_id, $meta_key, $meta_value);
  }
}

/**
 * This action hook is called whenever post metadata is added to a post.
 *
 * The PaySwarm-specific code is run in order to prevent duplicate metadata
 * in the database as well as to sanitize the metadata before it is written
 * to the database. It also ensures asset and listing signatures are fresh.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $meta_id the ID of the metadata.
 */
function payswarm_added_postmeta($meta_id) {
  $meta = get_post_meta_by_id($meta_id);
  $revision = wp_is_post_revision($meta->post_id);

  // do not perform the signature update/sanitize if the post is a revision
  // or if the metadata is not PaySwarm metadata
  if(!$revision && (strpos($meta->meta_key, '_payswarm_') === 0)) {
    // delete any older meta_id from the database for any PaySwarm metadata
    payswarm_database_remove_old_metadata(
      $meta->post_id, $meta_id, $meta->meta_key);

    // sanitize and update metadata
    payswarm_update_post_meta(
      $meta->post_id, $meta->meta_key, $meta->meta_value);

    // force a new signature for the post info
    payswarm_update_post_info($meta->post_id);
  }
}

/**
 * This action hook is called whenever post metadata is updated for a post.
 *
 * The PaySwarm-specific code sanitizes the metadata before it is written
 * to the database. It also ensures asset and listing signatures are fresh.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $meta_id the ID of the metadata.
 */
function payswarm_updated_postmeta($meta_id) {
  $meta = get_post_meta_by_id($meta_id);
  $revision = wp_is_post_revision($meta->post_id);

  // do not perform the signature update/sanitize if the post is a revision
  // or if the metadata is not PaySwarm metadata
  if(!$revision && (strpos($meta->meta_key, '_payswarm_') === 0)) {
    // sanitize and update metadata
    payswarm_update_post_meta(
      $meta->post_id, $meta->meta_key, $meta->meta_value);

    // force a new signature for the post info
    payswarm_update_post_info($meta->post_id);
  }
}

/* end of file, omit ?> */
