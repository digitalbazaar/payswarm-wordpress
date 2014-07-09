<?php

require_once('payswarm-wp-hooks.inc');
require_once('payswarm-database.inc');
require_once('payswarm-session.inc');
require_once('payswarm-admin.inc');
require_once('jsonld.php');

/**
 * Includes all CSS and script files that are needed by the
 * public-facing website.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_enqueue_scripts() {
  wp_enqueue_style('payswarm-style', PAYSWARM_PLUGIN_URL . 'payswarm.css');
  wp_enqueue_script('payswarm', PAYSWARM_PLUGIN_URL . 'payswarm.js');
}

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

  try {
    // get purchase url
    $info = payswarm_get_post_info($post_id);
    $callback = plugins_url() . '/payswarm/payswarm-access.php?p=' . $post_id;
    $url = @payswarm_get_purchase_url(
      $host, $info['listing']->id, $info['listing_hash'], $callback);

    // redirect to the PaySwarm Authority payment URL
    header('HTTP/1.1 303 See Other');
    header("Location: $url");
  }
  catch(Exception $e) {
    // output simple error page
    ?>
    <!DOCTYPE html>
    <html>
      <body>
        <h3>Connection Error</h3>
        <p>
          An error occurred while trying to contact your PaySwarm Authority
          (<a href="https://<?php echo $host; ?>"><?php echo $host ?></a>).
          Please try again or contact your PaySwarm Authority's customer
          support.
        </p>
      </body>
    </html>
    <?php
  }
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
  try {
    // get receipt from response
    $receipt = payswarm_get_receipt($response);

    // get contract info
    $contract = $receipt->contract;
    $identity_id = $contract->assetAcquirer;
    $asset = $contract->asset;
    $license = $contract->license;

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
  <html><body>
  <script type="text/javascript" src="<?php echo $script; ?>"></script>
  <script type="text/javascript">closePopup('<?php echo $url; ?>');</script>
  </body></html>
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
  if(!$asset) {
    return true;
  }

  // old asset format, consider stale
  if(!isset($asset->signature)) {
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
  $created = date_create($asset->signature->created);
  if($created <= $resign) {
    return true;
  }

  return false;
}

/**
 * Sets default payswarm price meta data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post.
 */
function payswarm_set_price_defaults($post_id) {
  if(!get_option('payswarm_registered', false)) {
    // not registered with payswarm yet, bail out
    return;
  }

  // if there's already a new price, return
  $new_price = get_post_meta($post_id, '_payswarm_price', true);
  if($new_price !== '') {
    return;
  }

  // look for old meta data price
  $old_price = get_post_meta($post_id, 'payswarm_price', true);

  if(PAYSWARM_PRODUCTION && $old_price === '') {
    return;
  }

  // use default price if no old price is present
  if($old_price === '') {
    // in production mode, return because default prices are not
    // automatically set
    if(PAYSWARM_PRODUCTION) {
      return;
    }

    // get defaults
    $options = payswarm_get_options();
    $price = $options['default_price'];
    $authority_rate = $options['default_authority_rate'];
  }
  // use old values
  else {
    $price = $old_price;
    $authority_rate = get_post_meta($post_id, 'payswarm_auth_rate', true);
  }

  // sanitize and update post meta data
  payswarm_update_post_meta(
    $post_id, '_payswarm_price', $price);
  payswarm_update_post_meta(
    $post_id, '_payswarm_authority_rate', $authority_rate);
  payswarm_update_post_meta(
    $post_id, '_payswarm_use_default_license', 1);

  // update post
  payswarm_update_post_info($post_id);

  // remove old post meta
  delete_post_meta($post_id, 'payswarm_asset_hash');
  delete_post_meta($post_id, 'payswarm_asset_signature');
  delete_post_meta($post_id, 'payswarm_auth_rate');
  delete_post_meta($post_id, 'payswarm_content_license_hash');
  delete_post_meta($post_id, 'payswarm_content_license_url');
  delete_post_meta($post_id, 'payswarm_listing_hash');
  delete_post_meta($post_id, 'payswarm_listing_signature');
  delete_post_meta($post_id, 'payswarm_listing_valid_from');
  delete_post_meta($post_id, 'payswarm_listing_valid_until');
  delete_post_meta($post_id, 'payswarm_signature_created');
  delete_post_meta($post_id, 'payswarm_use_default_license');
  delete_post_meta($post_id, 'payswarm_price');
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
  // ensure default prices are set
  payswarm_set_price_defaults($post_id);

  // get asset
  $asset = get_post_meta($post_id, '_payswarm_asset', true);
  if(is_string($asset)) {
    $asset = json_decode($asset);
  }

  // if asset is stale, force update
  $update = payswarm_is_asset_stale($asset);
  if(!$update) {
    // get listing
    $listing = get_post_meta($post_id, '_payswarm_listing', true);
    if(is_string($listing)) {
      $listing = json_decode($listing);
    }

    // if listing payees do not contain payee groups, force update
    foreach($listing->payee as $payee) {
      if(!property_exists($payee, 'payeeGroup')) {
        $update = true;
        break;
      }
    }

    // if listing is not valid, force update
    $update = $update || !payswarm_is_listing_valid($listing);
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
  $options = payswarm_get_options();

  // create asset
  $asset = (object)array(
    '@context' => payswarm_create_default_jsonld_context(),
    'id' => $post_url . '#asset',
    'type' => array('Asset', 'pto:WebPage'),
    'creator' => (object)array('name' => $post_author),
    'title' => $post_title,
    'assetProvider' => get_option('payswarm_vendor_id'),
    'assetContent' => $post_url,
    'listingRestrictions' => (object)array(
      // only allow a specific vendor to sell this asset
      'vendor' => get_option('payswarm_vendor_id')
    )
  );

  // sign asset
  payswarm_sign($asset, array(
    'key' => payswarm_wp_get_private_key(),
    'keyId' => payswarm_wp_get_public_key_id
  ));

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
  if(!$license_id || !$license_hash) {
    $license_id = $options['default_license_id'];
    $license_hash = $options['default_license_hash'];
  }

  // create listing
  $listing = (object)array(
    '@context' => payswarm_create_default_jsonld_context(),
    'id' => $post_url . '#listing',
    'type' => array('Listing', 'gr:Offering'),
    'vendor' => get_option('payswarm_vendor_id'),
    'payee' => array(
      (object)array(
        'id' => $post_url . '#listing-payee',
        'type' => 'Payee',
        'destination' => get_option('payswarm_financial_account'),
        'currency' => 'USD',
        'payeeGroup' => 'vendor',
        'payeeRate' => get_post_meta($post_id, '_payswarm_price', true),
        'payeeRateType' => 'FlatAmount',
        'payeeApplyType' => 'ApplyExclusively',
        'comment' => "Payment for $post_title by $post_author."
      )
    ),
    'payeeRule' => array(
      (object)array(
        'type' => 'PayeeRule',
        'payeeGroupPrefix' => array('authority'),
        'maximumPayeeRate' => get_post_meta(
          $post_id, '_payswarm_authority_rate', true),
        'payeeRateType' => 'Percentage',
        'payeeApplyType' => 'ApplyInclusively'
      )
    ),
    'asset' => $asset->id,
    'assetHash' => payswarm_hash($asset),
    'license' => $license_id,
    'licenseHash' => $license_hash,
    'validFrom' => $valid_from,
    'validUntil' => $valid_until
  );

  // sign listing
  payswarm_sign($listing, array(
    'key' => payswarm_wp_get_private_key(),
    'keyId' => payswarm_wp_get_public_key_id
  ));


  // hash listing
  $listing_hash = payswarm_hash($listing);

  // update post meta
  update_post_meta($post_id, '_payswarm_asset', json_encode($asset));
  update_post_meta($post_id, '_payswarm_listing', json_encode($listing));
  update_post_meta($post_id, '_payswarm_listing_hash', $listing_hash);

  return array(
    'asset' => $asset,
    'listing' => $listing,
    'listing_hash' => $listing_hash);
}

/**
 * Gets the HTML for an Access button.
 *
 * @param integer $post_id the ID of the post that is being expressed.
 * @param string $access_text the text to display next to the access button.
 */
function payswarm_get_access_button_html(
  $post_id, $access_text='View the full article for') {
  // if a price is available, add RDFa and access button
  $amount = get_post_meta($post_id, '_payswarm_price', true);
  if($amount !== '') {
    return '<div class="purchase section">' .
      '<div class="money row">' .
      payswarm_get_post_info_rdfa($post_id, $access_text) .
      '</div></div>';
  }

  // no purchase price available
  // tell the visitor that the article cannot be purchased
  $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';
  return '<div class="purchase section">' .
    '<div class="money payswarm-center">' .
    '<span class="label">' .
    __('The author has not set the price for the paid content of this post.') .
    '</span>' .
    '</div></div>';
}

/**
 * Generates the PaySwarm post information HTML+RDFa markup, which can be
 * inserted into parts of the WordPress site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id the ID of the post that is being expressed.
 * @param string $access_text the text to display next to the access button.
 *
 * @return string the HTML markup, surrounded by a DIV element, that contains
 *         the necessary RDFa markup to express the PaySwarm information.
 */
function payswarm_get_post_info_rdfa($post_id, $access_text) {
  // get payswarm post info
  $info = payswarm_get_post_info($post_id);

  // get URLs (add post ID to access URL as query param)
  $access_url = PAYSWARM_PLUGIN_URL . 'payswarm-access.php';
  $access_url = add_query_arg('p', $post_id, $access_url);
  $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';

  $session = payswarm_get_session();
  if($session === false || $session['identity_id'] === null) {
    $button_text = 'Access';
  }
  else {
    $button_text = 'Buy';
  }

  // compact using only prefixes
  $ctx = (object)array(
    '@context' => (object)array(
      'ccard' => 'https://w3id.org/commerce/creditcard#',
      'com' => 'https://w3id.org/commerce#',
      'dc' => 'http://purl.org/dc/terms/',
      'foaf' => 'http://xmlns.com/foaf/0.1/',
      'gr' => 'http://purl.org/goodrelations/v1#',
      'pto' => 'http://www.productontology.org/id/',
      'ps' => 'https://w3id.org/payswarm#',
      'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#',
      'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#',
      'sec' => 'https://w3id.org/security#',
      'vcard' => 'http://www.w3.org/2006/vcard/ns#',
      'xsd' => 'http://www.w3.org/2001/XMLSchema#'));
  $options = array('compactArrays' => false);
  $listing = jsonld_compact($info['listing'], $ctx, $options)->{'@graph'}[0];
  $asset = jsonld_compact($info['asset'], $ctx, $options)->{'@graph'}[0];

  $prefixes = array();
  foreach($ctx->{'@context'} as $prefix => $iri) {
    $prefixes[] = "xmlns:$prefix=\"$iri\"";
  }

  // convenience method for getting RDFa values from JSON-LD data
  $rdfa_getter = function($data) use (&$rdfa_getter) {
    return function() use ($data) {
      // use arguments as path into data
      $rval = $data;
      $args = func_get_args();
      $last_arg = null;
      foreach($args as $arg) {
        // default to first item in array
        if(is_array($rval) && is_string($arg)) {
          $rval = $rval[0];
        }

        if(is_array($rval)) {
          $rval = $rval[$arg];
        }
        else {
          $rval = $rval->{$arg};
        }

        $last_arg = $arg;
      }

      if(is_array($rval)) {
        // join types
        if($last_arg === '@type') {
          $rval = join(' ', $rval);
        }
        // default to first item in array
        else {
          $rval = $rval[0];
        }
      }

      // default to returning @value or @id for objects
      if(is_object($rval)) {
        if(property_exists($rval, '@value')) {
          $rval = $rval->{'@value'};
        }
        else if(property_exists($rval, '@id')) {
          $rval = $rval->{'@id'};
        }
      }

      // escape for html
      if(!is_object($rval)) {
        $rval = htmlentities($rval);
      }

      return $rval;
    };
  };

  // set vars for template
  $payee = $listing->{'com:payee'}[0];
  $payee_getter = $rdfa_getter($payee);
  $payee_rule = $listing->{'com:payeeRule'}[0];
  $vars = array(
    'prefixes' => implode(' ', $prefixes),
    'asset' => $rdfa_getter($asset),
    'listing' => $rdfa_getter($listing),
    'listing_hash' => $info['listing_hash'],
    'access_url' => $access_url,
    'pslogo_url' => $pslogo_url,
    'payee' => $payee_getter,
    'payee_rate' => sprintf('%1.02f', $payee_getter('com:payeeRate')),
    'payee_rule' => $rdfa_getter($payee_rule),
    'text_button' => __($button_text),
    'text_access' => __($access_text),
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

  // ensure default prices are set
  payswarm_set_price_defaults($post_id);

  // tags
  $begin_paid_tag = 'BEGIN_PAYSWARM_PAID_CONTENT';
  $end_paid_tag = 'END_PAYSWARM_PAID_CONTENT';
  $begin_unpaid_tag = 'BEGIN_PAYSWARM_UNPAID_ONLY_CONTENT';
  $end_unpaid_tag = 'END_PAYSWARM_UNPAID_ONLY_CONTENT';

  // tag regexes
  $begin_ws = '(?:(?:\\s*?<(?:(?:p)|(?:br\\/))>)|(?:\\s+|^))';
  $end_ws = '(?:\\s*?<(?:(?:\\/p)|(?:br\\s*?\\/))>)?';
  $begin_paid = "(?:$begin_ws($begin_paid_tag)$end_ws)";
  $end_paid = "(?:$begin_ws($end_paid_tag)$end_ws)";
  $begin_unpaid = "(?:$begin_ws($begin_unpaid_tag)$end_ws)";
  $end_unpaid = "(?:$begin_ws($end_unpaid_tag)$end_ws)";
  $deprecated_paid = '(<!--\\s*payswarm\\s*-->)';
  $begin_paid = "(?:$deprecated_paid|$begin_paid)";
  $end_paid = "(?:$deprecated_paid|$end_paid)";
  $paid = "{$begin_paid}(.*?)(?:{$end_paid}|{$begin_unpaid}|$)";
  $unpaid = "{$begin_unpaid}(.*?)(?:{$end_unpaid}|{$begin_paid}|$)";

  // full capture pattern (match eols with '.' with '/s' modifier)
  $pattern = "/(?:{$paid})|(?:{$unpaid})/s";

  // split content into sections
  $sections = array();
  $offset = 0;
  do {
    $result = preg_match(
      $pattern, $content, $matches, PREG_OFFSET_CAPTURE, $offset);

    if(!$result) {
      $type = 'free';
      $section = substr($content, $offset);
      $offset = strlen($content);
    }
    else {
      // 1 <!-- payswarm -->
      // 2 BEGIN_PAYSWARM_PAID_CONTENT
      // 3 paid content
      // 4 <!-- payswarm -->
      // 5 END_PAYSWARM_PAID_CONTENT
      // 6 BEGIN_PAYSWARM_UNPAID_ONLY_CONTENT paid terminator
      // 7 BEGIN_PAYSWARM_UNPAID_ONLY_CONTENT
      // 8 unpaid content
      // 9 END_PAYSWARM_UNPAID_ONLY_CONTENT

      // paid content found
      if(isset($matches[3]) && $matches[3][1] !== -1) {
        $type = 'paid';
        $index = 3;
      }
      // unpaid content found
      else if(isset($matches[8]) && $matches[8][1] !== -1) {
        $type = 'unpaid';
        $index = 8;
      }

      if($offset !== $matches[0][1]) {
        // add 'free' section before match
        $sections[] = array(
          'type' => 'free',
          'content' => substr($content, $offset, $matches[0][1] - $offset));
      }

      $section = $matches[$index][0];

      // an end-tag was provided
      if((isset($matches[4]) && $matches[4][1] !== -1) ||
        (isset($matches[5]) && $matches[5][1] !== -1) ||
        (isset($matches[9]) && $matches[9][1] !== -1)) {
        // skip entire match
        $offset = $matches[0][1] + strlen($matches[0][0]);
      }
      else {
        // skip content only
        $offset = $matches[$index][1] + strlen($section);
      }
    }

    $sections[] = array('type' => $type, 'content' => $section);
  }
  while($offset < strlen($content));

  // generate processed content from sections
  $processed_content = '';
  $paid_content = false;
  foreach($sections as $section) {
    if($section['type'] === 'paid') {
      $paid_content = true;
    }
    if($section['type'] === 'free' ||
      ($section['type'] === 'paid' && $purchased) ||
      ($section['type'] === 'unpaid' && !$purchased)) {
      $processed_content .= $section['content'];
    }
  }

  // inject all access buttons
  $access_button = "/{$begin_ws}PAYSWARM_ACCESS_BUTTON(?: ([\\w!?.\\s]*))?{$end_ws}/s";
  $access_button_html = payswarm_get_access_button_html($post_id, '');

  $create_button = function($matches) use ($post_id) {
    $access_text = isset($matches[1]) ? $matches[1] : '';
    return payswarm_get_access_button_html($post_id, $access_text);
  };

  // FIXME: temporarily limit access buttons to 1 until RDFa duplication
  // bug is fixed
  $processed_content = preg_replace_callback(
    $access_button, $create_button, $processed_content, 1, $count);

  // append access button by default if none were provided via markup
  // and the article has not been purchased and has paid content
  if($count === 0 && !$purchased && $paid_content) {
    $processed_content .= payswarm_get_access_button_html($post_id);
  }

  return $processed_content;
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
  $payee = $info['listing']->payee[0];
  $payee_rule = $info['listing']->payeeRule[0];

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
        value="<?php echo $payee->payeeRate; ?>" />
      <span>The price for access to non-free content.</span>
    </p>

    <label for="payswarm_authority_rate">
      <?php _e('Authority Percentage'); echo ' (%)'; ?>:
    </label>
    <p>
      <input type="text" id="payswarm_authority_rate"
        name="payswarm_authority_rate"
        value="<?php echo $payee_rule->maximumPayeeRate; ?>" />
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
          value="<?php echo $info['listing']->license; ?>" />
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
  if($autosave || $revision || !$edit_allowed) {
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
  // special-case license ID/hash because values are tied together
  if(($meta_key === '_payswarm_license_id' or
    $meta_key === '_payswarm_license_hash') && $meta_value === '') {
    $options = payswarm_get_options();
    $license_id = $options['default_license'];
    $license_hash = $options['default_license_hash'];
    update_post_meta(
      $post_id, '_payswarm_license_id', $license_id);
    update_post_meta(
      $post_id, '_payswarm_license_hash', $license_hash);
  }
  else {
    if($meta_key === '_payswarm_price') {
      if(!is_numeric($meta_value) || floatval($meta_value) < 0) {
        $options = payswarm_get_options();
        $meta_value = $options['default_price'];
      }
    }
    else if($meta_key === '_payswarm_authority_rate') {
      if(!is_numeric($meta_value) || floatval($meta_value) < 0) {
        $options = payswarm_get_options();
        $meta_value = $options['default_authority_rate'];
      }
    }

    // update using sanitized meta value
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
  $meta = get_metadata_by_mid('post', $meta_id);
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
  $meta = get_metadata_by_mid('post', $meta_id);
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
