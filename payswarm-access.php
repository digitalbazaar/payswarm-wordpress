<?php
require_once('../../../wp-config.php');
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');
require_once('payswarm-oauth.inc');

// FIXME: Check to ensure connection was made via SSL connection

// get the session ID that is associated with this request
$session = payswarm_check_session();

// get the scope that is associated with this request
$scope = 'payswarm-token';
// FIXME: implement $scope = 'payswarm-registration';

// retrieve the PaySwarm token, creating it if it doesn't already exist
$ptoken = payswarm_database_get_token($session, $scope, true);

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
      $post = $_GET['p'];
      $price = get_post_meta($post, 'payswarm_price', true);
      $request_url = get_option('payswarm_request_url') . 
         "?scope=$scope&currency=USD&balance=$price";
      $details = '{"balance": 0.0, "authorized_posts": ""}';

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
         $details = '{"balance": 0.0, "authorized_posts": ""}';

         payswarm_oauth1_authorize(
            $oauth, $session, $scope, $access_url, $details);
      }
      else
      {
         // if access was denied, print out an appropriate error
         $post = $_GET['p'];
         payswarm_access_denied($post);
      }
   }
   else if($ptoken['scope'] === 'payswarm-token' && 
      $ptoken['state'] === 'valid')
   {
      // State: authorized - we can just use the stored access token
      $contracts_url = get_option('payswarm_contracts_url');
      $oauth->setToken($ptoken['token'], $ptoken['secret']);
      $post = $_GET['p'];
      $price = get_post_meta($post, 'payswarm_price', true);
      $content_license_url = 
         get_post_meta($post, 'payswarm_content_license_url', true);
      // FIXME: We need to get the license hash by looking up the post_meta
      // license URL in a database and getting the hash from that.
      $content_license_hash = 
         get_option('payswarm_default_license_hash');

      // create the asset 
      $params = array(
         'asset' => get_permalink($post),
         'license' => $content_license_url,
         'license_hash' => $content_license_hash,
         'currency' => get_option('payswarm_default_currency'),
         'amount' => $price);

      // catch any token revocations
      try
      {
         $oauth->fetch($contracts_url, $params);
         
         // check to see if the purchase was approved and get the remaining
         // balance on the payment token
         $authorized = false;
         $balance = '0.0';
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
            $details = json_decode($ptoken['details'], true);

            // update the balance
            $details['balance'] = $balance;

            // update the list of authorized posts
            $posts = explode(' ', $details['authorized_posts']);
            array_push($posts, "$post");
            $posts = array_unique($posts);
            $details['authorized_posts'] = implode(' ', $posts);

            $tok['session'] = $ptoken['session'];
            $tok['state'] = $ptoken['state'];
            $tok['token'] = $ptoken['token'];
            $tok['secret'] = $ptoken['secret'];
            $tok['balance'] = $balance;
            $tok['details'] = json_encode($details);

            // Save the payment token and secret
            if(payswarm_database_update_token($tok))
            {
               $post = $_GET['p'];
               // FIXME: Generate the proper post path
               $redir_url = site_url() . '/?p=' . $post;
               header("Location: $redir_url");
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
      }
   }
}
catch(OAuthException $E)
{
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" . 
      print_r($err, true) . '</pre>');
}

function payswarm_access_denied($post)
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

    <p><a href="' . site_url() . "/?p=$post" . 
      '">Go back to the article preview</a>.</p>
  </div>
</div>';
   
   get_footer();
}
?>
