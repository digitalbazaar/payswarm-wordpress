<?php

add_action('wp_print_styles', 'payswarm_add_stylesheets');
add_filter('the_content', 'payswarm_filter_paid_content');
add_action('add_meta_boxes', 'payswarm_add_meta_boxes');
add_action('save_post', 'payswarm_save_post_data');

function payswarm_add_stylesheets()
{
  $css_url = WP_PLUGIN_URL . '/' .
     str_replace(basename( __FILE__), '', plugin_basename(__FILE__)) .
     '/payswarm.css';

   wp_register_style('payswarm-style', $css_url);
   wp_enqueue_style( 'payswarm-style');
}

function payswarm_filter_paid_content($content)
{
   global $post;
   $processed_content = $content;
   $article_purchased = false;
   $amount = get_post_meta($post->ID, 'payswarm_price', true);

   if(!$article_purchased)
   {
      $temp = explode('<!--payswarm-->', $content, 2);
      $processed_content = $temp[0];
      $paid_content_exists = (count($temp) > 1) and (strlen($temp[1] > 0));

      $currency = "USD";
      $currency_symbol = "$";

      $pslogo_url = WP_PLUGIN_URL . '/' .
         str_replace(basename( __FILE__), '', plugin_basename(__FILE__)) .
         '/images/payswarm-20.png';

      if($amount !== "" and $paid_content_exists)
      {
         $processed_content .= '<div class="purchase section">
             <div class="money row"> 
             <span class="label">' . __('Purchase the full article') . 
             '</span> ' . $currency . " " . $currency_symbol . $amount . 
             '<button class="purchase-button"> 
               <img src="' . $pslogo_url .
             '">'. __('Purchase') .'</button>
           </div></div>';
      }
      else if($paid_content_exists)
      {
         $processed_content .= '<div class="purchase section">
             <div class="money row"> 
             <span class="label">' . 
                __('The author has not set the price for the paid content.') .
             '</span><button class="purchase-button"> 
               <img src="' . $pslogo_url .
             '">'. __('Cannot Purchase') .'</button>
           </div></div>';
      }
   }

   return $processed_content;
}

function payswarm_add_meta_boxes()
{
   add_meta_box('payswarm_sectionid', __( 'PaySwarm Options'), 
                'payswarm_create_meta_box', 'post', 'side', 0);
}

function payswarm_create_meta_box()
{
   // nonce is required for fields to prevent forgery attacks
   wp_nonce_field(plugin_basename(__FILE__), 'payswarm_price_nonce');

   // FIXME: Get the currency symbol from the database
   global $post;
   $price = get_post_meta($post->ID, 'payswarm_price', true);
   $currency_symbol = "$";

   echo '<div><strong><label for="payswarm_price_field">' .
      __("Price") . 
      ' </label></strong>';
   echo $currency_symbol;
   echo '<input type="text" id= "payswarm_price_field" ' .
      'name="payswarm_price" value="'. $price . '" size="6" />' .
      '<span>The price to charge for access to the non-free content.</span>' .
      '</div>';
}

function payswarm_save_post_data($post_id)
{
   $rval = $post_id;

   // check whether or not the nonce is valid, the post is being autosaved
   // and whether or not editing is allowed
   $valid_nonce = wp_verify_nonce($_POST['payswarm_price_nonce'],
      plugin_basename(__FILE__));
   $autosaving = defined('DOING_AUTOSAVE') && DOING_AUTOSAVE;
   $edit_allowed = current_user_can('edit_post', $post_id);

   // Only save the data if we're not autosaving, the nonce is valid and
   // if the current user can perform edits
   if(!$autosaving && $valid_nonce && $edit_allowed)
   {
      // Retrieve the price from the post data
      $price = $_POST['payswarm_price'];

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
      else if(!add_post_meta($post_id, 'payswarm_price', $price, true))
      {
         update_post_meta($post_id, 'payswarm_price', $price);
      }
   }

   return $rval;
}

?>
