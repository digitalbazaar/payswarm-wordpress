<?php
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');
require_once('payswarm-article.inc');

// get post to access
$post_id = $_GET['p'];

print "FIXME: Implement new purchase flow...";

// PURCHASE REQUEST

// TODO: UI to select the PaySwarm Authority if one isn't already selected

// digitally sign the purchase request

// create a POST form including the purchase request targeted at the PA

// PURCHASE RESPONSE

// accept the response from the PA and decrypt it

// decrypt the response, and if it is valid, approve access to the article

// store session ID for person from purchase response.

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
