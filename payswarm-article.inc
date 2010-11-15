<?php

/**
 * Adds the required stylesheets for the PaySwarm plugin.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_add_stylesheets()
{
   $css_url = PAYSWARM_PLUGIN_URL . 'payswarm.css';

   wp_register_style('payswarm-style', $css_url);
   wp_enqueue_style( 'payswarm-style');
}

/**
 * Generates the PaySwarm asset information HTML+RDFa markup, which can be 
 * inserted into parts of the WordPress site.
 * 
 * @package payswarm
 * @since 1.0
 *
 * @param string $post_id The post ID that is being expressed.
 * @return string The HTML markup, surrounded by a DIV element, that contains
 *         all of the necessary RDFa markup to express the asset information.
 */
function payswarm_asset_information_rdfa($post_id)
{
   $post_url = get_permalink($post_id);
   $asset_provider = 'http://example.org/people#bob';
   $payswarm_authority = 'https://' . get_option('payswarm_authority');
   $now_iso8601 = date_format(date_create('now'), DateTime::ISO8601);
   $one_day_from_now = date_add(date_create('now'), 
      date_interval_create_from_date_string('P1D'));
   $one_day_from_now_iso8601 = 
      date_format($one_day_from_now, DateTime::ISO8601);

   $rval = "
      <div xmlns:xsd=\"http://www.w3.org/2001/XMLSchema#\"
           xmlns:foaf=\"http://purl.org/dc/terms/\"
           xmlns:dcterms=\"http://purl.org/dc/terms/\"
           xmlns:ps=\"http://purl.org/payswarm#\"
           class=\"asset-info\"
           about=\"$post_url#asset\" typeof=\"ps:Asset ps:WebPage\"
           onclick=\"javascript:toggleAssetInformation();\">
         <div><strong>Offer details...</strong></div>
         <span property=\"dcterms:title\">The Title</span> by
         <span property=\"dcterms:creator\">Author Name</span>.
         <a rel=\"ps:contentUrl\" href=\"$post_url\">Permalink</a>. Offered by
         <a rel=\"ps:assetProvider\" href=\"$asset_provider\"
            ><span property=\"foaf:name\">Bob</span></a> at
         <a rel=\"ps:authority\" 
            href=\"$payswarm_authority\">PaySwarm Authority</a>.
         <div>This offer is valid from 
            <span property=\"ps:notValidBefore\"
               content=\"$now_iso8601\" datatype=\"xsd:dateTime\">now</span>
            until <span property=\"ps:notValidAfter\"
               content=\"$one_day_from_now_iso8601\" 
               datatype=\"xsd:dateTime\">tomorrow</span>.</div>
      </div>";

   return $rval;
}

/**
 * Filters the for-profit content for posts.
 * 
 * This filter is applied based on the value of the session and PaySwarm
 * Payment Token. If the visitor has not been cleared to view an article, only
 * the free content is displayed. If the visitor has paid for the content,
 * the proper article content is shown. This is a filter that is applied to
 * the 'the_content' WordPress filter.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $content The article content as it is stored in the database.
 * @return string The filtered article content after it has been modified by 
 *    this filter.
 */
function payswarm_filter_paid_content($content)
{
   global $post;
   $processed_content = $content;
   $post_purchased = false;

   // check to see if the article has been purchased already
   $post_id = '' . $post->ID;
   $session = payswarm_check_session(false);
   $ptoken = payswarm_database_get_token($session);
   if($ptoken != null)
   {
      $authorized_posts = explode(' ', $ptoken['authorized_posts']);
      if(in_array($post_id, $authorized_posts))
      {
         $post_purchased = true;
      }
   }

   // if the article has not been purchased, filter out all of the for-pay
   // article content.
   if(!$post_purchased)
   {
      $amount = get_post_meta($post->ID, 'payswarm_price', true);
      $temp = explode('<!--payswarm-->', $content, 2);
      $processed_content = $temp[0];
      $paid_content_exists = (count($temp) > 1) and (strlen($temp[1] > 0));

      $currency = get_option('payswarm_default_currency');
      $currency_symbol = "$";

      $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';

      if($amount !== "" and $paid_content_exists)
      {
         // FIXME: This replacement isn't exactly safe, if the URL contains
         // another 'http://' in it, but then again, can't think of any reason
         // that it would
         $access_url = preg_replace('/^http:\/\/(.*)$/', 'https://$1', 
            PAYSWARM_PLUGIN_URL . 'payswarm-access.php') . "?p=" . $post->ID;

         // if the article has not been purchased, and a price is available,
         // display the price and a purchase button.
         $processed_content .= '<div class="purchase section">
             <div class="money row">' .
             payswarm_asset_information_rdfa($post_id) . 
             '<span class="label">' . __('View the full article for ') . 
             $currency_symbol . $amount . '</span>' .
             '<form class="money" action="' . $access_url . '" ' .
             'method="post"><button class="purchase-button">' .
             '<img alt="'. __('Access') .'" src="' . $pslogo_url .
             '" />'. __('Access') .'</button></form>
           </div></div></p>';
      }
      else if($paid_content_exists)
      {
         // if the article has not been purchased, but a price is not available,
         // tell the visitor that the article cannot be purchased.
         $processed_content .= '<div class="purchase section">
             <div class="money row"> 
             <span class="label">' . 
                __('The author has not set the price for the paid content.') .
             '</span><button class="purchase-button"> 
               <img alt="'. __('Cannot Purchase') .'" src="' . $pslogo_url .
             '" />'. __('Cannot Purchase') .'</button>
           </div></div></p>';
      }
   }
   else
   {
      $processed_content = str_replace('<!--payswarm-->', '', $content);
   }

   return $processed_content;
}

/**
 * Adds the PaySwarm payment information metabox to a post editor box.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_add_meta_boxes()
{
   add_meta_box('payswarm_sectionid', __( 'PaySwarm Options'), 
                'payswarm_create_meta_box', 'post', 'side', 0);
}

/**
 * Generates the HTML for a PaySwarm post metadata editing box.
 * 
 * Echoes the price for a PaySwarm post metadata editing box. 
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_create_meta_box()
{
   // nonce is required for fields to prevent forgery attacks
   wp_nonce_field(plugin_basename(__FILE__), 'payswarm_price_nonce');

   // FIXME: Get the currency symbol from the database
   global $post;
   $price = get_post_meta($post->ID, 'payswarm_price', true);
   $currency_symbol = "$";

   // dump the HTML contents
   echo '<div><strong><label for="payswarm_price_field">' .
      __("Price") . 
      ' </label></strong>';
   echo $currency_symbol;
   echo '<input type="text" id= "payswarm_price_field" ' .
      'name="payswarm_price" value="'. $price . '" size="6" />' .
      '<span>The price to charge for access to the non-free content.</span>' .
      '</div>';
   echo '<div><strong><label for="payswarm_content_license_url_field">' .
      __("Content License") . 
      ' </label></strong>';
   echo '<select id="payswarm_content_license_url_field" 
      name="payswarm_content_license_url">
            <option value="' . get_option('payswarm_default_license_url') . 
      '">' . get_option('payswarm_default_license_url') . '</option>
      </select>
      <span>The license to associate with the content when it is sold.
      </span></div>';
}

/**
 * Saves the PaySwarm-specific meta-data that is associated with a post.
 * 
 * This method is called whenever a save is done for a post. It saves all of
 * the information that is specified in the PaySwarm options box for the post.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param integer $post_id The post identifier as recorded in the WordPress 
 *    database tables.
 * @return integer The post identifier as recorded in the WordPress database
 *    tables.
 */
function payswarm_save_post_data($post_id)
{
   $rval = $post_id;

   // check whether or not the nonce is valid, the post is being autosaved
   // and whether or not editing is allowed
   $valid_nonce = false;
   $autosaving = defined('DOING_AUTOSAVE') && DOING_AUTOSAVE;
   $edit_allowed = current_user_can('edit_post', $post_id);

   // get the nonce value if it exists
   if(isset($_POST['payswarm_price_nonce']))
   {
      $valid_nonce = wp_verify_nonce($_POST['payswarm_price_nonce'],
         plugin_basename(__FILE__));
   }

   // Only save the data if we're not autosaving, the nonce is valid and
   // if the current user can perform edits
   if(!$autosaving && $valid_nonce && $edit_allowed)
   {
      // Retrieve the price from the post data
      $price = $_POST['payswarm_price'];
      // Retrieve the license URL from the post data
      $license_url = $_POST['payswarm_content_license_url'];

      // converts the string value into a well-formed floating point value
      if(is_numeric($price))
      {
         $price = (string)floatval($price);
      }
      else
      {
         $price = "";
      }

      // Delete the post metadata if the value is effectively zero or
      // update the post metadata if the value is valid
      if($price == "" or $price == 0)
      {
         delete_post_meta($post_id, 'payswarm_price');
      }
      update_post_meta($post_id, 'payswarm_price', $price);
      update_post_meta($post_id, 'payswarm_content_license_url', $license_url);
   }

   return $rval;
}

?>
