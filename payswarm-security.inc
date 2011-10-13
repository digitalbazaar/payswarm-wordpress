<?php

require_once('jsonld.php');

/**
 * Gets the PaySwarm session that is being tracked on the site.
 *
 * @package payswarm
 * @since 1.0
 *
 * @return mixed the session ID for the active payswarm session or false if
 *         no such session exists.
 */
function payswarm_get_session()
{
   global $_COOKIE;
   $session = false;

   // check to see if the payswarm-session cookie exists
   if(array_key_exists('payswarm-session', $_COOKIE))
   {
      $session = $_COOKIE['payswarm-session'];
   }

   return $session;
}

/**
 * Creates a new PaySwarm session if one doesn't already exist. If one does
 * exist, it updates it if the "update" flag is set to true.
 *
 * The PaySwarm session is stored in a cookie 'payswarm-session'. This session
 * is used to associate the visitor with a PaySwarm Payment Token if one is
 * ever retrieved for the visitor.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param update true to update the PaySwarm session if already exists.
 *
 * @return string the session ID for the active PaySwarm session.
 */
function payswarm_create_session($update = true)
{
   global $_COOKIE;

   // try to get existing session
   $session = payswarm_get_session();

   // update cookie value if it doesn't exist
   if($session === false)
   {
      $update = true;
   }

   if($update)
   {
      $now = time();
      $timeout = 0;

      // no session exists, create new one
      if($session === false)
      {
         $random_value = rand(0, 100000);
         $session = sha1("$now$random_value");
      }
      // get timeout cookie
      else if(array_key_exists('payswarm-session-timeout', $_COOKIE))
      {
         $timeout = $_COOKIE['payswarm-session-timeout'];
      }

      // only update the cookie every 5 minutes since it requires a database
      // write (allows purging of expired sessions from the database)
      $now = time();
      if($now - 300 > $timeout)
      {
         // update session in database (updates expiration time)
         $timeout = $now + 3600;
         payswarm_database_update_session($session);

         // FIXME: if get_option("payswarm_ssl_on") then use https cookie, else
         // security is handled in a different way, so set cookie to non-https
         $secure = true;

         // update cookies
         global $_SERVER;
         setcookie(
            'payswarm-session', $session, $timeout, '/',
            $_SERVER['HTTP_HOST'], $secure);
         setcookie(
            'payswarm-timeout', $timeout, $timeout, '/',
            $_SERVER['HTTP_HOST'], $secure);
      }
   }

   return $session;
}

/**
 * Clears a PaySwarm session.
 *
 * Removes the 'payswarm-session' cookie by setting the value to empty
 * and setting the time an hour in the past.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_clear_session()
{
   // FIXME: if get_option("payswarm_ssl_on") then use https cookie, else
   // security is handled in a different way, so set cookie to non-https
   $secure = true;

   global $_SERVER;
   $expires = time() - 3600;
   setcookie('payswarm-session', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);
   setcookie('payswarm-timeout', '', $expires, '/',
      $_SERVER['HTTP_HOST'], $secure);
}

/**
 * Generates a hash of the JSON encoded data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj The JSON-LD object to hash.
 *
 * @return The SHA-1 hash of the encoded JSON data.
 */
function payswarm_generate_json_hash($obj)
{
   // SHA-1 hash JSON
   return sha1(payswarm_json_encode(jsonld_normalize($obj)));
}

/**
 * Signs JSON encoded data.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj The JSON-LD object to sign.
 * @param string $pem the PEM-encoded private key to sign with.
 * @param string $sig_date the ISO8601-formatted signature creation date.
 *
 * @return The base64-encoded signature of the encoded JSON data.
 */
function payswarm_sign_json($obj, $pem, $sig_date)
{
   // sign and return base64-encoded signature
   $json = payswarm_json_encode(jsonld_normalize($obj));
   openssl_sign($sig_date . $json, $signature, $pem, OPENSSL_ALGO_SHA1);
   return base64_encode($signature);
}

/**
 * Retrieves the PaySwarm Authority public key from the network or the
 * cache. The public key expires exactly 24 hours after it is retrieved in
 * order to ensure that a fairly recent public key is used.
 *
 * @return string a PEM-encoded public key for the PaySwarm Authority.
 */
function payswarm_get_pa_public_key()
{
   $rval = get_transient('payswarm_authority_public_key');

   // PaySwarm Authority public key should be refreshed
   if($rval === false)
   {
      $url = get_option('payswarm_authority_public_key_url');
      $response = wp_remote_get($url, array(
         'sslverify' => PAYSWARM_SSL_ENABLED,
         'headers' => array('Accept' => 'application/ld+json')));

      if(is_wp_error($response))
      {
         throw new Exception('Could not get PaySwarm Authority Public Key: ' .
            $response>get_error_message());
      }

      // Extract the key from the JSON-LD response
      $key = json_decode($response['body']);

      if($key !== NULL && isset($key->{'sec:publicKeyPem'}))
      {
         $rval = $key->{'sec:publicKeyPem'};
         
         // cache key for one day
         set_transient('payswarm_authority_public_key', $rval, 24*60*60);
      }
   }

   return $rval;
}

/**
 * Creates a nonce to be given to a PaySwarm Authority so it can
 * be returned in a signed and encrypted message.
 *
 * @return string the nonce to use.
 */
function payswarm_create_message_nonce()
{
   // get the current payswarm session
   $session = payswarm_get_session();

   // generate a unique 20-digit pseudo-random nonce
   do
   {
      $rval =
         base_convert(mt_rand(), 10, 36) .
         base_convert(mt_rand(), 10, 36);

      // save the nonce in the database with a 15 minute timeout
      if(get_transient($rval) === false)
      {
         set_transient($rval, $session, 15*60);
      }
   }
   while(get_transient($rval) !== $session);

   return $rval;
}

/**
 * Checks a message nonce. If the nonce exists and its associated payswarm
 * session matches the current payswarm session, then it is valid.
 *
 * @param string the nonce to check.
 *
 * @return true if the nonce is valid, false if not.
 */
function payswarm_check_message_nonce($nonce)
{
   if(isset($nonce) and $nonce !== null)
   {
      // compare current session w/nonce associated session
      $rval = (payswarm_get_session() === get_transient($nonce));

      // clean up nonce
      delete_transient($nonce);
   }
   else
   {
      // no nonce given
      $rval = false;
   }

   return $rval;
}

/**
 * Decrypts an encrypted PaySwarm message.
 *
 * @param stdClass $encrypted_message the message to decrypt.
 * @param string $pem the PEM-encoded private key to decrypt the message.
 *
 * @return string the decrypted message.
 */
function payswarm_decrypt_message($encrypted_message, $pem)
{
   if($encrypted_message->{'sec:algorithm'} !== 'rsa-aes-128-cbc')
   {
      $algorithm = $encrypted_message->{'sec:algorithm'};
      throw new Exception('PaySwarm Security Exception: ' .
         'Unknown encryption algorithm "' . $algorithm . '"');
   }

   // load private key from PEM
   $pkey = openssl_pkey_get_private($pem);
   if($pkey === false)
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'Failed to load the private key.');
   }

   // decrypt symmetric key
   $encryption_key = base64_decode($encrypted_message->{'sec:encryptionKey'});
   if(openssl_private_decrypt($encryption_key, $skey, $pkey) === false)
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'Failed to decrypt the encryption key.');
   }

   // decrypt IV
   $encIv = base64_decode($encrypted_message->{'sec:iv'});
   if(openssl_private_decrypt($encIv, $iv, $pkey) === false)
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'Failed to decrypt the initialization vector (IV).');
   }

   // free private key resource
   openssl_pkey_free($pkey);

   // decrypt data
   $data = openssl_decrypt(
      $encrypted_message->{'sec:data'}, 'aes128', $skey, false, $iv);

   if($data === false)
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'Failed to decrypt the encrypted message due to an incorrect ' .
         'symmetric key or an invalid initialization vector (IV).');
   }

   // decode JSON message
   return json_decode($data);
}

/**
 * Verifies a JSON-LD digitally signed PaySwarm message.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param stdClass $obj The JSON-LD object to verify.
 * @param string $pem the PEM-encoded public key to verify with.
 *
 * @return true if verified, false if not (exception thrown).
 */
function payswarm_verify_message($obj, $pem)
{
   $rval = false;

   // frame message to retrieve signature
   $frame = (object)array(
      '@context' => payswarm_create_default_jsonld_context(),
      'sec:signature' => (object)array(
         'dc:created' => new stdClass(),
         'sec:signatureValue' => new stdClass(),
         'sec:nonce' => new stdClass()
      )
   );
   $obj = jsonld_frame($obj, $frame);
   
   if($obj === NULL || !isset($obj->{'sec:signature'}))
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'The message is not digitally signed.');
   }

   // save signature property and remove from object
   $sprop = $obj->{'sec:signature'};
   unset($obj->{'sec:signature'});

   // check the message nonce
   if(!payswarm_check_message_nonce($sprop->{'sec:nonce'}))
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'The message nonce is invalid.');
   }

   // ensure signature timestamp is +/- 15 minutes
   $now = time();
   $time = date_create($sprop->{'dc:created'})->getTimestamp();
   if($time < ($now - 15*60) || $time > ($now + 15*60))
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'The message digital signature timestamp is out of range.');
   }

   // normalize and serialize the object
   $normalized = jsonld_normalize($obj);
   $json = payswarm_json_encode($normalized);

   // get the data to hash
   $data = $sprop->{'sec:nonce'} . $sprop->{'dc:created'} . $json;

   // decode the signature value
   $sig = base64_decode($sprop->{'sec:signatureValue'});

   // verify the signature
   $rc = openssl_verify($data, $sig, $pem, OPENSSL_ALGO_SHA1);
   if($rc === 1)
   {
      $rval = true;
   }
   else if($rc === -1)
   {
      // throw exception, error while trying to verify
      throw new Exception('PaySwarm Security Exception: ' .
         'Low-level API error: ' . openssl_error_string());
   }
   else
   {
      throw new Exception('PaySwarm Security Exception: ' .
         'The digital signature on the message is invalid.');
   }

   return $rval;
}

/**
 * Verifies a JSON-encoded, encrypted, digitally-signed PaySwarm message.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string json_message the json-encoded message to verify.
 *
 * @return the JSON-LD decoded message (exception thrown on error).
 */
function payswarm_decode_payswarm_authority_message($json_message)
{
   // convert message from json
   $msg = json_decode($json_message);

   // decrypt message
   $pem = get_option('payswarm_private_key');
   $rval = payswarm_decrypt_message($msg, $pem);

   // verify message
   $pem = payswarm_get_pa_public_key();
   payswarm_verify_message($rval, $pem);

   return $rval;
}

?>
