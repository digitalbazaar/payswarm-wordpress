<?php

add_action('wp_print_styles', 'payswarm_add_stylesheets');
add_filter('the_content', 'payswarm_filter_paid_content');
add_filter('the_title', 'payswarm_filter_title');

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
   $processed_content = $content;
   $article_purchased = false;

   if(!$article_purchased)
   {
      $temp = explode('<!--payswarm-->', $content, 2);
      $processed_content = $temp[0];

      $currency = "USD";
      $currency_symbol = "$";
      $amount = "0.01";

      $pslogo_url = WP_PLUGIN_URL . '/' .
         str_replace(basename( __FILE__), '', plugin_basename(__FILE__)) .
         '/images/payswarm-20.png';

      $processed_content .= '<div class="purchase section">
          <div class="money row"> 
          <span class="label">' . __('Purchase the full article') . '</span> ' .
          $currency . " " . $currency_symbol . $amount . 
          '<button class="purchase-button"> 
            <img src="' . $pslogo_url .
          '">'. __('Purchase') .'</button>
        </div></div>';
   }

   return $processed_content;
}

function payswarm_filter_title($content)
{
   $article_purchased = true;

   return $content . __(" (Preview)");
}
?>
