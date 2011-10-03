<?php
require_once('payswarm-utils.inc');
require_once('payswarm-database.inc');

// set to false for production mode
$payswarm_oauth1_debug_mode = true;

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

            // redirect to token authorization page on payswarm authority
            $authorize_url =
               $options['authorize_url'] . '?' . http_build_query($params);
            header("Location: $authorize_url");
            exit(0);
         }
         // retry purchase only once if resource was not found (likely
         // due to validity period rollover)
         else if(
            $retry == -1 and
            $err !== NULL and array_key_exists('type', $err) and
            $err->type == 'payswarm.database.NotFound')
         {
            $retry = 1;
         }
         else
         {
            // can't handle exception, re-throw it
            throw $E;
         }
      }
   }
   while($retry-- > 0);

   // check to see if the purchase was authorized
   $result = array();
   parse_str($oauth->getLastResponse(), $result);
   if(isset($result['authorized']) and $result['authorized'] === 'true')
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

?>
