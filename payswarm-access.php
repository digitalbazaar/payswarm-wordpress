<?php
require_once('../../../wp-config.php');
require_once('payswarm-utils.inc');

// force access to happen through SSL
payswarm_force_ssl();

require_once('payswarm-article.inc');
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');
require_once('payswarm-oauth.inc');

// state table
$states = array
(
   'initializing' => 'payswarm_get_request_token',
   'authorizing' => 'payswarm_get_access_token',
   'valid' => 'payswarm_access_purchase_post'
);

// get the session ID that is associated with this request
$session = payswarm_check_session();

// get the scope that is associated with this request
$scope = 'payswarm-payment';

// get a payswarm token, creating it if it doesn't already exist
$token = payswarm_database_get_token($session, $scope, true);

// if authorizing and no oauth_token is present, restart process
if($token['state'] === 'authorizing' and !isset($_GET['oauth_token']))
{
   $token['state'] = 'initializing';
}

// get post to access
$post_id = $_GET['p'];

try
{
   // setup OAuth client
   $client_id = get_option('payswarm_client_id');
   $client_secret = get_option('payswarm_client_secret');
   $oauth = new OAuth(
      $client_id, $client_secret, OAUTH_SIG_METHOD_HMACSHA1,
      OAUTH_AUTH_TYPE_FORM);

   // FIXME: Disable debug output for OAuth for production software
   // FIXME: Enable SSL checks for production software
   $oauth->enableDebug();
   $oauth->disableSSLChecks();

   // handle next token state
   call_user_func($states[$token['state']], $oauth);
}
catch(OAuthException $E)
{
   // FIXME: make user friendly error page
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" .
      print_r($err, true) . '</pre>');
}

/**
 * Gets a request token from the PaySwarm authority.
 *
 * @param Object $oauth the OAuth client.
 */
function payswarm_get_request_token($oauth)
{
   global $session, $scope, $token, $post_id;

   $price = get_post_meta($post_id, 'payswarm_price', true);
   $request_url = get_option('payswarm_request_url') .
      "?scope=$scope&currency=USD&balance=$price";
   $details = array('balance' => '0.00');
   payswarm_oauth1_initialize(
      $oauth, $session, $scope, $request_url, $details);
}

/**
 * Gets an access token from the PaySwarm authority.
 *
 * @param Object $oauth the OAuth client.
 */
function payswarm_get_access_token($oauth)
{
   global $session, $scope, $token, $post_id;

   // access approved (verifier returned from authority)
   if(array_key_exists('oauth_verifier', $_GET))
   {
      // get access token from authority
      $access_url = get_option('payswarm_access_url');
      $oauth->setToken($_GET['oauth_token'], $token['secret']);
      $details = array('balance' => '0.00');
      payswarm_oauth1_authorize(
         $oauth, $session, $scope, $access_url, $details);
   }
   // access denied
   else
   {
      payswarm_access_denied($post_id);
   }
}

/**
 * Prints out an appropriate error when getting an access token was denied
 * by the PaySwarm authority.
 *
 * @param Integer $post_id the ID of the post that access was denied to.
 */
function payswarm_access_denied($post_id)
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

/**
 * Requests the purchase of a post with the PaySwarm authority.
 *
 * @param Object $oauth the OAuth client.
 */
function payswarm_access_purchase_post($oauth)
{
   global $session, $scope, $token, $post_id;

   // State: authorized - we can use the stored access token
   $oauth->setToken($token['token'], $token['secret']);

   // catch any token revocations
   try
   {
      // setup service endpoint and parameters
      $contracts_url = get_option('payswarm_contracts_url');

      // use loop to allow retry of purchase if validity period rolled over
      $retry = -1;
      do
      {
         $info = payswarm_get_post_info($post_id);
         $params = array(
            'listing' => $info['listing_url'],
            'listing_hash' => $info['listing_hash']
         );

         try
         {
            // attempt to perform the purchase
            $oauth->fetch($contracts_url, $params);
         }
         catch(OAuthException $E)
         {
            // check to see if we got an insufficient funds exception
            $err = json_decode($oauth->getLastResponse());
            if($err !== NULL and array_key_exists('type', $err) and
               $err->type === 'payswarm.oauth1.InsufficientFunds')
            {
               // Attempt to recharge the already authorized OAuth token
               $price = get_post_meta($post_id, 'payswarm_price', true);
               $authorize_url = get_option('payswarm_authorize_url');
               $oauth_token = $token['token'];
               $redir_url = urlencode(payswarm_get_current_url());
               $authorize_url = "$authorize_url?oauth_token=$oauth_token" .
                  "&oauth_callback=$redir_url&balance=$price";
               header("Location: $authorize_url");
               exit(0);
            }
            // retry purchase only once if resource was not found (likely
            // due to validity period rollover)
            else if(
               $err !== NULL and array_key_exists('type', $err) and
               $err->type == 'payswarm.database.NotFound')
            {
               $retry = ($retry == -1) ? 1 : 0;
            }
            else
            {
               // if no insufficient funds exception, re-throw the exception
               throw $E;
            }
         }
      }
      while($retry-- > 0);

      // check to see if the purchase was approved and get the remaining
      // balance on the payment token
      $authorized = false;
      $balance = '0.00';
      // FIXME: use standard form encoded data parsing
      $items = explode('&', $oauth->getLastResponse());

      foreach($items as $item)
      {
         $kv = explode('=', $item, 2);
         if($kv[0] === 'authorized' and $kv[1] === 'true')
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
         // update the balance
         $token['details']['balance'] = $balance;

         // save the payment token and authorize the post
         if(payswarm_database_update_token($token) and
            payswarm_database_authorize_post($token, $post_id))
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
      $invalidToken = strpos($error, 'payswarm.database.NotFound');

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

?>
