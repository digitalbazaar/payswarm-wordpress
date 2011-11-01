<?php

// NOTE: DO not move this block of code, we need to extract the encrypted
// message if it exists before including wp-config.php
if(isset($_POST["encrypted-message"]))
{
   $json_message = $_POST["encrypted-message"];

   // make sure to remove magic quotes if in use
   if(get_magic_quotes_gpc())
   {
      $json_message = stripcslashes($json_message);
   }
}

require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');
require_once('payswarm-security.inc');
require_once('payswarm-article.inc');

// if no message was posted, show purchase request form
if(!isset($json_message))
{
   // get post to access
   $post_id = $_GET['p'];

   // TODO: UI to select the PaySwarm Authority if one isn't already selected
   //$pa_url = payswarm_get_payment_url_cookie();
   //if($pa_url === false)
   //{
   //   // get PA url from a UI
   //   $config_url = payswarm_config_get_pa_config_url($pa_url);
   //   $cfg = payswarm_config_get_pa_config($config_url);
   //   payswarm_set_payment_url_cookie(cfg['ps:contractsService']);
   //}

   // FIXME: use the payment URL for the customer, not the merchant
   // get the payment URL for the PaySwarm Authority
   $payment_url = get_option("payswarm_payment_url");

   // create the purchase request
   $info = payswarm_get_post_info($post_id);

   // create a GET form including the purchase request targeted at the PA
   echo payswarm_purchase_form($payment_url, $info);
}
// handle posted message
else
{
   payswarm_handle_purchase_response($json_message);
}

/**
 * Generates the purchase form given a post information object.
 *
 * @param string $payment_url the PA payment URL.
 * @param array $info the information about the particular post.
 *
 * @return string the purchase form.
 */
function payswarm_purchase_form($payment_url, $info)
{
   $title = $info['post_title'];
   $author = $info['post_author'];
   $listing_url = $info['listing_url'];
   $listing_hash = $info['listing_hash'];
   $callback_url = plugins_url() . '/payswarm/payswarm-access.php';
   $response_nonce = payswarm_create_message_nonce();

   $rval = <<<FORM
<html>
<head>
<title>Purchase $title by $author</title>
</head>
<h1>Purchase $title by $author</h1>
<p>Do you want to purchase $title by $author?</p>
<form method="GET" action="$payment_url">
<input type="hidden" name="listing" value="$listing_url" />
<input type="hidden" name="listing-hash" value="$listing_hash" />
<input type="hidden" name="callback" value="$callback_url" />
<input type="hidden" name="response-nonce" value="$response_nonce" />
<input type="submit" value="Yes" />
<input type="button" value="No" />
<p><em>If you have previously purchased this item you won't be charged twice
for it.</em></p>
</form>
</html>
FORM;

   return $rval;
}

/**
 * Handles the purchase response.
 *
 * @param string $json_message the POSTED purchase response.
 */
function payswarm_handle_purchase_response($json_message)
{
   // decode json-encoded, encrypted message
   $msg = payswarm_decode_payswarm_authority_message($json_message);

   // check message type
   if(payswarm_jsonld_has_type($msg, 'err:Error'))
   {
      // FIXME: call access denied instead of exception?
      //payswarm_access_denied();
      throw new Exception('PaySwarm Purchase Exception: ' .
         $msg->{'err:message'});
   }
   else if(!payswarm_jsonld_has_type($msg, 'ps:Contract'))
   {
      throw new Exception('PaySwarm Purchase Exception: ' .
         'Invalid purchase response from PaySwarm Authority.');
   }

   // validate contract
   if(!property_exists($msg, 'ps:assetAcquirer') or
      !property_exists($msg, 'ps:asset') or
      !property_exists($msg, 'ps:license'))
   {
      throw new Exception('PaySwarm Purchase Exception: ' .
         'Unknown Contract format.');
   }

   // get contract info
   $profile_id = $msg->{'ps:assetAcquirer'};
   $asset = $msg->{'ps:asset'};
   $license = $msg->{'ps:license'};

   // get post ID from asset URL (strip after hash)
   $post_id = url_to_postid(preg_replace('/#.*/', '', $asset));
   if($post_id === 0)
   {
      throw new Exception('PaySwarm Purchase Exception: ' .
         'The Asset in the Contract could not be matched to a post.');
   }

   // create/update payswarm session
   $session = payswarm_create_session($profile_id);

   // authorize the post and redirect to it
   payswarm_database_authorize_post($profile_id, $post_id, $license);
   header('Location: ' . get_permalink($post_id));
   exit(0);
}

/**
 * Prints out an appropriate error when getting an access to a post was denied
 * by the PaySwarm authority.
 */
function payswarm_access_denied()
{
   global $post_id;

   // FIXME: Unfortunately, this generates a PHP Notice error for
   // WP_Query::$is_paged not being defined. Need to figure out which file
   // declares that variable.
   get_header();

   echo '
<div class="category-uncategorized">
  <h2 class="entry-title">Access Denied to PaySwarm Article</h2>
  <div class="entry-content">
    <p>
      Access to the article was denied because this website was not
      allowed to access your PaySwarm account. This usually happens because
      you did not allow this website to access your PaySwarm provider
      information.
    </p>

    <p><a href="' . get_permalink($post_id) .
      '">Go back to the article preview</a>.</p>
  </div>
</div>';

   get_footer();
}

?>
