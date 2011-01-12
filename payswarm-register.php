<?php
require_once('../../../wp-config.php');
require_once('payswarm-session.inc');
require_once('payswarm-database.inc');
require_once('payswarm-oauth.inc');

// FIXME: Check to ensure connection was made via SSL connection

// the session ID that is associated with this request
$session = 'payswarm-registration';

// get the scope that is associated with this request
$scope = 'payswarm-registration';

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
      $request_url = get_option('payswarm_request_url') . "?scope=$scope";
      $details = '{}';

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
         $details = '{}';

         payswarm_oauth1_authorize(
            $oauth, $session, $scope, $access_url, $details);
      }
      else
      {
         // if access was denied, print out an appropriate error
         payswarm_registration_denied($post);
      }
   }
   else if($ptoken['scope'] === 'payswarm-registration' && 
      $ptoken['state'] === 'valid')
   {
      // State: authorized - we can just use the stored access token
      $key_registration_url = get_option('payswarm_key_registration_url');
      $preferences_url = get_option('payswarm_preferences_url');

      $oauth->setToken($ptoken['token'], $ptoken['secret']);

      // FIXME: Register the public key using the OAuth registration token
      print_r($ptoken);
      echo "REGISTER_PUBLIC_KEY";
      
      // FIXME: Get the default financial account and currency information
      echo "GET DEFAULT FINANCIAL ACCOUNT";
      
      // FIXME: Get the default license and license hash
      echo "Get DEFAULT LICENSE AND HASH";
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
  <h2 class="entry-title">Access Denied when Registering</h2> 
  <div class="entry-content"> 
    <p>
      Access to the PaySwarm registration information was denied because this 
      website was not allowed to access your PaySwarm account. This usually 
      happens because you did not allow this website to access your 
      PaySwarm account information by not assigning it a Registration Token.
    </p>

    <p><a href="' . site_url() . "/wp-admin/plugins.php?page=payswarm" . 
      '">Go back to the administrative page</a>.</p>
  </div>
</div>';
   
   get_footer();
}
?>
