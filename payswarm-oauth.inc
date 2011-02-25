<?php
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');

// set to false for production mode
$payswarm_oauth1_debug_mode = true;

/**
 * Gets a new OAuth access token from the PaySwarm authority.
 *
 * Once the OAuth process completes, either the valid callback or the denied
 * callback will be called along with the PaySwarm options given here. The
 * options will also include 'token', the PaySwarm token, and 'session', the
 * current PaySwarm session.
 *
 * This method may throw an OAuthException in the event of an error.
 *
 * @param array $options the PaySwarm OAuth options:
 *           client_id: the ID of the OAuth client.
 *           client_secret: the secret of the OAuth client.
 *           scope: the scope for the token.
 *           details: initialization token details to use.
 *           request_params: parameters to pass to the request url.
 *           request_url: the authority's OAuth request url.
 *           authorize_url: the authority's OAuth authorize url.
 *           access_url: the authority's OAuth access url.
 *           success: a callback for a valid access token: callback($options).
 *           denied: a callback for a denied access token: callback($options).
 */
function payswarm_oauth1_get_token($options)
{
   // state table
   $states = array
   (
      'initializing' => 'payswarm_oauth1_get_request_token',
      'authorizing' => 'payswarm_oauth1_get_access_token',
      'valid' => $options['success'],
      'denied' => $options['denied']
   );

   // get the session ID that is associated with this request
   $session = payswarm_check_session();

   // get a payswarm token, creating it if it doesn't already exist
   $token = payswarm_database_get_token($session, $options['scope'], true);

   // restart token process if token was previously denied or if authorizing
   // and no oauth_token is present
   if($token['state'] === 'denied' or
      ($token['state'] === 'authorizing' and !isset($_GET['oauth_token'])))
   {
      $token['state'] = 'initializing';
   }

   // add token and session to options
   $options['token'] = $token;
   $options['session'] = $session;

   // handle next token state
   while(true)
   {
      $token = $options['token'];
      $options = call_user_func($states[$token['state']], $options);
      if(!is_array($options))
      {
         exit(0);
      }
   }
}

/**
 * Creates an OAuth client.
 *
 * @param String $client_id the OAuth client ID.
 * @param String $client_secret OAuth client secret.
 */
function payswarm_create_oauth_client($client_id, $client_secret)
{
   // use OAuth SHA-1 HMAC signature method
   $oauth = new OAuth(
      $client_id, $client_secret,
      OAUTH_SIG_METHOD_HMACSHA1, OAUTH_AUTH_TYPE_FORM);

   // handle debug mode
   global $payswarm_oauth1_debug_mode;
   if($payswarm_oauth1_debug_mode)
   {
      $oauth->enableDebug();
      $oauth->disableSSLChecks();
   }

   return $oauth;
}

/**
 * Gets a request token from the PaySwarm authority. This call will redirect
 * the user agent.
 *
 * @param array $options the PaySwarm OAuth options.
 */
function payswarm_oauth1_get_request_token($options)
{
   // get session and scope
   $token = $options['token'];
   $session = $token['session'];
   $scope = $token['scope'];

   // build authority request url
   $params = $options['request_params'];
   $params['scope'] = $scope;
   $request_url = $options['request_url'] . '?' . http_build_query($params);

   // create callback url for authority to redirect to
   $callback_url = add_query_arg(
      'session', $session, payswarm_get_current_url());

   // get request token from authority
   $oauth = payswarm_create_oauth_client(
      $options['client_id'], $options['client_secret']);
   $result = $oauth->getRequestToken($request_url, $callback_url);

   // store updated token
   $token['state'] = 'authorizing';
   $token['details'] = $options['details'];
   $token['token'] = $result['oauth_token'];
   $token['secret'] = $result['oauth_token_secret'];
   if(payswarm_database_update_token($token))
   {
      // redirect to authority's OAuth authorize url to get the request token
      // authorized (which will allow an access token to be retrieved)
      $authorize_url = $options['authorize_url'];
      $oauth_token = $token['token'];
      header("Location: $authorize_url?oauth_token=$oauth_token");
   }
   else
   {
      // FIXME: can this produce an infinite redirect problem?

      // if something went wrong, clear the cookie and try again
      global $_SERVER;
      setcookie('payswarm-session', $session, time() - 3600, '/',
          $_SERVER['HTTP_HOST'], true);
      header('Location: ' . payswarm_get_current_url());
   }

   exit(0);
}

/**
 * Gets an access token from the PaySwarm authority.
 *
 * @param array $options the PaySwarm OAuth options.
 *
 * @return array $options the updated PaySwarm OAuth options to allow the next
 *         token state to be processed immediately.
 */
function payswarm_oauth1_get_access_token($options)
{
   // access approved if verifier returned from authority
   if(array_key_exists('oauth_verifier', $_GET))
   {
      // get access token from authority
      $token = $options['token'];
      $access_url = $options['access_url'];
      $oauth = payswarm_create_oauth_client(
         $options['client_id'], $options['client_secret']);
      $oauth->setToken($_GET['oauth_token'], $token['secret']);
      $result = $oauth->getAccessToken($access_url);

      // update token
      $token['state'] = 'valid';
      $token['details'] = $options['details'];
      $token['details']['customer_id'] = $result['customer_id'];
      $token['token'] = $result['oauth_token'];
      $token['secret'] = $result['oauth_token_secret'];
   }
   else
   {
      // access denied
      $token['denied'] = true;
   }

   // store token
   payswarm_database_update_token($token);
   return $options;
}

/**
 * Requests the purchase of a post with the PaySwarm authority.
 *
 * This method may throw an OAuthException in the event of an error.
 *
 * @param array $options the PaySwarm OAuth options:
 *           client_id: the ID of the OAuth client.
 *           client_secret: the secret of the OAuth client.
 *           token: the PaySwarm token.
 *           contracts_params: parameters to pass to the contracts url.
 *           authorize_params: parameters to pass to the authorize url.
 *           contracts_url: the authority's OAuth contracts url.
 *           authorize_url: the authority's OAuth authorize url.
 *           success: a callback for success: callback($options).
 */
function payswarm_oauth1_purchase_post($options)
{
   // create OAuth client
   $token = $options['token'];
   $oauth = payswarm_create_oauth_client(
      $options['client_id'], $options['client_secret']);
   $oauth->setToken($token['token'], $token['secret']);

   // use loop to allow retry of purchase once if validity period rolled over
   $retry = -1;
   do
   {
      try
      {
         // attempt to perform the purchase
         $oauth->fetch($options['contracts_url'], $options['contracts_params']);
      }
      catch(OAuthException $E)
      {
         // check to see if we got an insufficient funds exception
         $err = json_decode($oauth->getLastResponse());
         if($err !== NULL and array_key_exists('type', $err) and
            $err->type === 'payswarm.oauth1.InsufficientFunds')
         {
            // attempt to recharge the already authorized OAuth token
            $params = array(
               'oauth_token' => $token['token'],
               // FIXME: make oauth_callback an option?
               'oauth_callback' => payswarm_get_current_url()
            );
            $params = array_merge($params, $options['authorize_params']);
            $authorize_url =
               $options['authorize_url'] . '?' . http_build_query($params);
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
            // if not insufficient funds exception, re-throw the exception
            throw $E;
         }
      }
   }
   while($retry-- > 0);

   // check to see if the purchase was authorized
   $result = array();
   parse_str($oauth->getLastResponse(), $result);
   if($result['authorized'] === 'true')
   {
      // update token
      $token['details']['balance'] = $result['balance'];
      payswarm_database_update_token($token);
      call_user_func($options['success'], $options);
   }
   else
   {
      call_user_func($options['denied'], $options);
   }
}


// FIXME: OLD FUNCTIONS BELOW



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
   $tok['details']['customer_id'] = $access_token_info['customer_id'];

   // save the access token and secret
   if(payswarm_database_update_token($tok))
   {
      $redir_url = payswarm_get_current_url();
      header("Location: $redir_url");
   }
}

?>
