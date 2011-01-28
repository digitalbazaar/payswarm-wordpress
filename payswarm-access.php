<?php
require_once('../../../wp-config.php');
require_once('payswarm-article.inc');
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');
require_once('payswarm-oauth.inc');

// FIXME: Check to ensure connection was made via SSL connection

// get the session ID that is associated with this request
$session = payswarm_check_session();

// get the scope that is associated with this request
$scope = 'payswarm-payment';

// retrieve the PaySwarm token, creating it if it doesn't already exist
$ptoken = payswarm_database_get_token($session, $scope, true);

$post_id = $_GET['p'];

// If we are authorizing, then there should be an oauth_token, if not, start
// the process over.
if($ptoken['state'] === 'authorizing' && !isset($_GET['oauth_token']))
{
   $ptoken['state'] = 'initializing';
}

try
{
   // setup the OAuth client
   $client_id = get_option('payswarm_client_id');
   $client_secret = get_option('payswarm_client_secret');
   $oauth = new OAuth(
      $client_id, $client_secret, OAUTH_SIG_METHOD_HMACSHA1, 
      OAUTH_AUTH_TYPE_FORM);

   // FIXME: Disable debug output for OAuth for production software
   $oauth->enableDebug();

   // FIXME: Enable SSL checks for production software
   $oauth->disableSSLChecks();

   // check the state of the payment token
   if($ptoken['state'] === 'initializing')
   {
      $price = get_post_meta($post_id, 'payswarm_price', true);
      $request_url = get_option('payswarm_request_url') . 
         "?scope=$scope&currency=USD&balance=$price";
      $details = array
      (
         'balance' => '0.0',
         'authorized_posts' => ''
      );

      payswarm_oauth1_initialize(
         $oauth, $session, $scope, $request_url, $details);
   }
   else if($ptoken['state'] === 'authorizing')
   {
      // State 1 - Handle callback from PaySwarm
      if(array_key_exists('oauth_verifier', $_GET))
      {
         // get and store an access token
         $access_url = get_option('payswarm_access_url');
         $oauth->setToken($_GET['oauth_token'], $ptoken['secret']);
         $details = array
         (
            'balance' => '0.0',
            'authorized_posts' => ''
         );
         
         payswarm_oauth1_authorize(
            $oauth, $session, $scope, $access_url, $details);
      }
      else
      {
         // if access was denied, print out an appropriate error
         payswarm_access_denied($post_id);
      }
   }
   else if($ptoken['scope'] === 'payswarm-payment' && 
      $ptoken['state'] === 'valid')
   {
      // State: authorized - we can use the stored access token
      $oauth->setToken($ptoken['token'], $ptoken['secret']);
      
      // catch any token revocations
      try
      {
         // setup service endpoint and parameters
         $contracts_url = get_option('payswarm_contracts_url');
         $info = payswarm_get_post_info($post_id, true);
         $params = array(
            'listing' => $info['listing_url'],
            'listing_hash' => $info['listing_hash']
         );
         
         $oauth->fetch($contracts_url, $params);
         
         // check to see if the purchase was approved and get the remaining
         // balance on the payment token
         $authorized = false;
         $balance = '0.0';
         // FIXME: use standard form encoded data parsing
         $items = explode('&', $oauth->getLastResponse());

         foreach($items as $item)
         {
            $kv = explode('=', $item, 2);
            if($kv[0] === 'authorized' && $kv[1] === 'true')
            {
               $authorized = true;
            }
            else if($kv[0] === 'balance')
            {
               $balance = $kv[1];
            }
         }

         if($authorized)
         {
            // append this post to the array of authorized posts associated
            // with the payment token
            $details = $ptoken['details'];

            // update the balance
            $details['balance'] = $balance;

            // update the list of authorized posts
            $posts = explode(' ', $details['authorized_posts']);
            array_push($posts, "$post_id");
            $posts = array_unique($posts);
            $details['authorized_posts'] = implode(' ', $posts);

            $tok['session'] = $ptoken['session'];
            $tok['scope'] = $ptoken['scope'];
            $tok['state'] = $ptoken['state'];
            $tok['token'] = $ptoken['token'];
            $tok['secret'] = $ptoken['secret'];
            $tok['details'] = $details;

            // Save the payment token and secret
            if(payswarm_database_update_token($tok))
            {
               header('Location: ' . get_permalink($post_id));
            }
         }
      }
      catch(OAuthException $E)
      {
         // if there is an error, check to see that the token has been
         // revoked
         $error = $oauth->getLastResponse();
         $invalidToken = strpos($error, 'bitmunk.database.NotFound');

         // if the token is invalid, start the process over
         if($invalidToken !== false)
         {
            global $_SERVER;
            setcookie('payswarm-session', $session, time() - 3600, '/',  
               $_SERVER['HTTP_HOST'], true);
            header('Location: ' . payswarm_get_current_url());
         }
         else
         {
            // re-raise
            throw $E;
         }
      }
   }
}
catch(OAuthException $E)
{
   // FIXME: make user friendly error page
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" . 
      print_r($err, true) . '</pre>');
}

function payswarm_access_denied($post_id)
{
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
