<?php

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

      if(!is_wp_error($response))
      {
         // FIXME: Extract the key from the JSON-LD response
         $rval = <<<PUBLIC_KEY
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqMbYknpvLLx6+ZQ3IucP
l6dsEdSj82CBw9Xd7GQIsD7qYIzE18LKe9I+SroxHhDDpBuwTZREV9kOwyvOcvbD
bp46+ymA7TGIRoScz6L7e8QSCqEPg/z6FBWtsCNpVx+AUF68Ci99IBU0xWKHyPRp
6ZHpW9ET4150Q3ZFQLcw7xD8pt9lCb7YGbmWcZWYvMysLRZ4ihuYCbbaBzgtTp3i
QQGmrZ2gcQVwdx898/OcJ8Kj9PNJEyoydoqcIQtVyQtfKev+Ofegy6pfH69i5+Z3
Oqs2Ochr3tVnzPAMIVsvW/eVtnXacyxUsyT+m2uhRtC+e72zlDmobpLPm7RPYGJA
kQIDAQAB
-----END PUBLIC KEY-----
PUBLIC_KEY;

         // key expires in one day
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
   if(isset($nonce))
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
      throw new Exception('PaySwarm Decryption Exception: ' .
         'Unknown encryption algorithm "' . $algorithm . '"');
   }

   // load private key from PEM
   $pkey = openssl_pkey_get_private($pem);
   if($pkey === false)
   {
      throw new Exception('PaySwarm Decryption Exception: ' .
         'Failed to load the private key.');
   }

   // decrypt symmetric key
   $encryption_key = base64_decode($encrypted_message->{'sec:encryptionKey'});
   if(openssl_private_decrypt($encryption_key, $skey, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception: ' .
         'Failed to decrypt the encryption key.');
   }

   // decrypt IV
   $encIv = base64_decode($encrypted_message->{'sec:iv'});
   if(openssl_private_decrypt($encIv, $iv, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception: ' .
         'Failed to decrypt the initialization vector (IV).');
   }

   // free private key resource
   openssl_pkey_free($pkey);

   // decrypt data
   $data = openssl_decrypt(
      $encrypted_message->{'sec:data'}, 'aes128', $skey, false, $iv);

   if($data === false)
   {
      throw new Exception('PaySwarm Decryption Exception: ' .
         'Failed to decrypt the encrypted message due to an incorrect ' .
         'symmetric key or an invalid initialization vector.');
   }

   // decode JSON message
   return json_decode($data);
}

function payswarm_decode_payswarm_authority_message($json_message)
{
   // convert message from json
   $msg = json_decode($json_message);

   // decrypt message
   $pem = get_option('payswarm_private_key');
   $rval = payswarm_decrypt_message($msg, $pem);

   // FIXME: remove me
   print_r($rval);

   // verify message
   $pem = payswarm_get_pa_public_key();
   if(!payswarm_verify_json($rval, $pem))
   {
      // FIXME: remove me/make exception
      print_r('invalid message signature');
      $rval = false;
   }
   else
   {
      // FIXME: do json-ld frame
      // $rval = jsonld_frame($ctx, $rval);

      // check message nonce
      $nonce = $rval->{'sec:signature'}->{'sec:nonce'};
      if(!payswarm_check_message_nonce($nonce))
      {
         // FIXME: remove me/make exception
         print_r('invalid message nonce');
         $rval = false;
      }
      else
      {
         // ensure timestamp is +/- 15 minutes
         $now = time();
         $time = date_create($rval->{'sec:signature'}->{'dc:created'});
         if($time < ($now - 15*60) || $time > ($now + 15*60))
         {
            // FIXME: remove me/make exception
            print_r('message timestamp out of range');
            $rval = false;
         }
      }
   }

   return $rval;
}

?>
