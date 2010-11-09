<?php
require_once('../../../wp-config.php');
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');

// FIXME: Check to ensure connection was made via SSL connection

// get the session ID that is associated with this request
$session = payswarm_check_session();

// retrieve the PaySwarm token, creating it if it doesn't already exist
$ptoken = payswarm_database_get_token($session, true);

// If we are authorizing, then there should be an oauth_token, if not, start
// the process over.
if($ptoken['state'] === 'authorizing' && !isset($_GET['oauth_token']))
{
   $ptoken['state'] = 'initializing';
}

try
{
   $client_id = get_option('payswarm_client_id');
   $client_secret = get_option('payswarm_client_secret');

   $oauth = new OAuth(
      $client_id, $client_secret, OAUTH_SIG_METHOD_HMACSHA1, 
      OAUTH_AUTH_TYPE_FORM);

   // enable debug output for OAuth and remove SSL checks
   $oauth->enableDebug();
   $oauth->disableSSLChecks();

   // check the state of the payment token
   if($ptoken['state'] === 'initializing')
   {
      // Initializing state - Generate request token and redirect user to 
      // payswarm site to authorize
      $request_url = get_option('payswarm_request_url');
      $post = $_GET['p'];
      $callback_url = payswarm_get_current_url() . "&session=$session";
      $request_token_info = 
         $oauth->getRequestToken("$request_url?currency=USD&amount=1.00", 
         $callback_url);

      $tok['session'] = $session;
      $tok['token'] = $request_token_info['oauth_token'];
      $tok['secret'] = $request_token_info['oauth_token_secret'];
      $tok['amount'] = '0.0';
      $tok['state'] = 'authorizing';
      if(payswarm_database_update_token($tok))
      {
         $authorize_url = get_option('payswarm_authorize_url');
         // Save the token and the secret, which will be used later
         $oauth_token = $tok['token'];
         header("Location: $authorize_url?oauth_token=$oauth_token");
      }
      else
      {
         // if something went wrong, clear the cookie and attempt the purchase
         // again
         global $_SERVER;
         setcookie('payswarm-session', $session, time() - 3600, '/', 
             $_SERVER['HTTP_HOST'], true);
         header('Location: ' . payswarm_get_current_url());
      }
   }
   else if($ptoken['state'] === 'authorizing')
   {
      // State 1 - Handle callback from payswarm 
      if(array_key_exists('oauth_verifier', $_GET))
      {
         // get and store an access token
         $access_url = get_option('payswarm_access_url');
         $oauth->setToken($_GET['oauth_token'], $ptoken['secret']);
         $access_token_info = $oauth->getAccessToken($access_url);
         $tok['session'] = $session;
         $tok['state'] = 'valid';
         $tok['token'] = $access_token_info['oauth_token'];
         $tok['secret'] = $access_token_info['oauth_token_secret'];
         $tok['amount'] = '0.0';

         // save the access token and secret
         if(payswarm_database_update_token($tok))
         {
            $redir_url = payswarm_get_current_url();
            header("Location: $redir_url");
         }
      }
      else
      {
         $fh = fopen("articles/denied.html", "r");
         print(fread($fh, 32768));
         fclose($fh);
      }
   }
   else if($ptoken['state'] === 'valid')
   {
      // State: authorized - we can just use the stored access token
      $contracts_url = get_option('payswarm_contracts_url');
      $oauth->setToken($ptoken['token'], $ptoken['secret']);
      $post = $_GET['p'];
      $price = get_post_meta($post, 'payswarm_price', true);
      $params = array(
         'asset' => site_url() . '/?p=' . $post,
         // FIXME: Generate the correct PaySwarm license
         'license' => 'http://example.org/licenses/personal-use',
         'license_hash' => '866f3f9540e572e8cc4467f470a869242db201ba',
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
            $posts = explode(' ', $ptoken['authorized_posts']);
            array_push($posts, "$post");
            $posts = array_unique($posts);

            $tok['session'] = $ptoken['session'];
            $tok['state'] = $ptoken['state'];
            $tok['token'] = $ptoken['token'];
            $tok['secret'] = $ptoken['secret'];
            $tok['amount'] = $balance;
            $tok['authorized_posts'] = implode(' ', $posts);

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
         $invalidToken = strpos($error, "bitmunk.database.NotFound");

         if($invalidToken !== false)
         {
            global $_SERVER;
            setcookie(
               'payswarm-session', $session, time() - 3600, '/',  
               $_SERVER['HTTP_HOST'], true);
            $fh = fopen("articles/revoked.html", "r");
            print(fread($fh, 32768));
            fclose($fh);
         }
      }
   }
}
catch(OAuthException $E)
{
   $err = json_decode($E->lastResponse);
   print_r('<pre>' . $E . "\nError details: \n" . print_r($err, true) . '</pre>');
}

?>
