<?php
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');

// force access to happen over SSL
payswarm_force_ssl();

require_once('payswarm-article.inc');
require_once('payswarm-oauth.inc');

// get post to access
$post_id = $_GET['p'];

// get payswarm token
try
{
   payswarm_oauth1_get_token(array(
      'client_id' => get_option('payswarm_client_id'),
      'client_secret' => get_option('payswarm_client_secret'),
      'scope' => 'payswarm-payment',
      'details' => array('balance' => '0.00'),
      'request_params' => array(
         'currency' => 'USD',
         'balance' => get_post_meta($post_id, 'payswarm_price', true)
      ),
      'request_url' => get_option('payswarm_request_url'),
      'authorize_url' => get_option('payswarm_authorize_url'),
      'access_url' => get_option('payswarm_access_url'),
      'success' => 'payswarm_access_purchase_post',
      'denied' => 'payswarm_access_denied'
   ));
}
catch(OAuthException $E)
{
   // FIXME: catch oauth exception and redirect to payswarm website?

   // FIXME: make user friendly error page
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" .
      print_r($err, true) . '</pre>');
}

/**
 * Requests the purchase of a post with the PaySwarm authority.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_access_purchase_post($options)
{
   global $post_id;

   // build options for purchasing post
   $info = payswarm_get_post_info($post_id);
   $options['contracts_params'] = array(
      'listing' => $info['listing_url'],
      'listing_hash' => $info['listing_hash']
   );
   $options['authorize_params'] = array(
      'balance' => get_post_meta($post_id, 'payswarm_price', true)
   );
   $options['contracts_url'] = get_option('payswarm_contracts_url');
   $options['success'] = 'payswarm_access_post_authorized';
   // FIXME: keep same denial handler?
   //$options['denied'] = 'payswarm_access_post_denied';

   try
   {
      // purchase post
      payswarm_oauth1_purchase_post($options);
   }
   catch(OAuthException $E)
   {
      // FIXME: handle this error within payswarm-oauth function(s)?
      // FIXME: can this produce an infinite redirect problem?
      throw $E;

      // if there is an error, check to see if the token has been revoked
      $error = $E->lastResponse;
      $invalidToken = strpos($error, 'payswarm.database.NotFound');

      // if the token is invalid, start the process over
      if($invalidToken !== false)
      {
         // clear the session and redirect to the current page
         payswarm_clear_session();
         header('Location: ' . payswarm_get_current_url());
      }
      else
      {
         // re-raise
         throw $E;
      }
   }
}

/**
 * Called when access to a post has been authorized.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_access_post_authorized($options)
{
   global $post_id;

   // authorize the post and redirect to it
   payswarm_database_authorize_post($options['token'], $post_id);
   header('Location: ' . get_permalink($post_id));
   exit(0);
}

/**
 * Prints out an appropriate error when getting an access token was denied
 * by the PaySwarm authority.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_access_denied($options)
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
