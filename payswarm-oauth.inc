<?php

/**
 * Performs OAuth token initialization and if successful, writes the token
 * result to the database.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param object $oauth the OAuth object as created by the OAuth API.
 * @param string $session The session that should be associated with the
 *    OAuth credential.
 * @param string $scope the scope for the OAuth token - either payswarm-payment
 *    or payswarm-registration
 * @param string $request_url the full request URL for the OAuth token request.
 * @param string $details the details that should be stored along with the 
 *    OAuth token in the WordPress database.
 */
function payswarm_oauth1_initialize(
   $oauth, $session, $scope, $request_url, $details)
{
   // Initializing state - Generate request token and redirect user to 
   // payswarm site to authorize
   $callback_url =
      add_query_arg('session', $session, payswarm_get_current_url());

   $request_token_info = 
      $oauth->getRequestToken($request_url, $callback_url);

   $tok['session'] = $session;
   $tok['scope'] = $scope;
   $tok['state'] = 'authorizing';
   $tok['token'] = $request_token_info['oauth_token'];
   $tok['secret'] = $request_token_info['oauth_token_secret'];
   $tok['details'] = $details;
   if(payswarm_database_update_token($tok))
   {
      // Save the token and the secret, which will be used later
      $authorize_url = get_option('payswarm_authorize_url');
      $oauth_token = $tok['token'];

      header("Location: $authorize_url?oauth_token=$oauth_token");
   }
   else
   {
      
      print_r($tok);
      die("ERROR");
      // if something went wrong, clear the cookie and try again
      global $_SERVER;

      setcookie('payswarm-session', $session, time() - 3600, '/', 
          $_SERVER['HTTP_HOST'], true);
      header('Location: ' . payswarm_get_current_url());
   }
}

/**
 * Validates authorization on a request token. 
 *
 * @package payswarm
 * @since 1.0
 *
 * @param object $oauth the OAuth object as created by the OAuth API.
 * @param string $session The session that should be associated with the
 *    OAuth credential.
 * @param string $scope the scope for the OAuth token - either payswarm-payment
 *    or payswarm-registration
 * @param string $request_url the full request URL for the OAuth token request.
 * @param string $details the details that should be stored along with the 
 *    OAuth token in the WordPress database.
 */
function payswarm_oauth1_authorize(
   $oauth, $session, $scope, $access_url, $details)
{
   $access_token_info = $oauth->getAccessToken($access_url);
   $tok['session'] = $session;
   $tok['scope'] = $scope;
   $tok['state'] = 'valid';
   $tok['token'] = $access_token_info['oauth_token'];
   $tok['secret'] = $access_token_info['oauth_token_secret'];
   $tok['details'] = $details;
   
   // save the access token and secret
   if(payswarm_database_update_token($tok))
   {
      $redir_url = payswarm_get_current_url();
      header("Location: $redir_url");
   }
}

?>
