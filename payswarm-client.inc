<?php

/**
 * Retrieves the PaySwarm Authority public key from the network or the 
 * cache. The public key expires exactly 24 hours after it is retrieved in
 * order to ensure that a fairly recent public key is used.
 *
 * @return A PEM-encoded public key for the PaySwarm Authority.
 */
function payswarm_get_pa_public_key()
{
   $rval = null;
   $refresh_key = false;
   
   // check to see if the PaySwarm Authority public key should be refreshed
   if(!get_option('payswarm_authority_public_key_expiration') ||
      !get_option('payswarm_authority_public_key'))
   {
      $refresh_key = true;
   }
   else
   {
      $expiration = 
         intval(get_option('payswarm_authority_public_key_expiration'));
      if(time() >= $expiration)
      {
         $refresh_key = true;
      }
   }
   
   // if the key should be refreshed, fetch if from the network
   if($refresh_key)
   {
      $public_key = "";
      $pa_public_key_url = get_option('payswarm_authority_public_key_url');
      $response = wp_remote_get($pa_public_key_url, array(
         'sslverify' => PAYSWARM_SSL_ENABLED,
         'headers' => array('Accept' => 'application/ld+json')));
         
      if(!is_wp_error($response))
      {
         // FIXME: Extract the key from the JSON-LD response
         $public_key = <<<PUBLIC_KEY
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
         update_option('payswarm_authority_public_key_expiration', 
            time() + (24 * 60 * 60));
         update_option('payswarm_authority_public_key', $public_key);
      }
   }

   // read the public key from the database
   $rval = get_option('payswarm_authority_public_key');

   return $rval;
}

/**
 * Decrypts an encrypted PaySwarm message.
 *
 * @param stdClass $encoded_message the message to decrypt.
 * @param string $pem the PEM-encoded private key to decrypt the message.
 *
 * @return string the decrypted message.
 */
function payswarm_decrypt_message($encoded_message, $pem)
{
   if($encoded_message->{'sec:algorithm'} !== 'rsa-aes-128-cbc')
   {
      $algorithm = $encoded_message->{'sec:algorithm'};
      throw new Exception('PaySwarm Decryption Exception:' .
         'Unknown encryption algorithm: ' . $algorithm);
   }

   // load private key from PEM
   $pkey = openssl_pkey_get_private($pem);
   if($pkey === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to load the private key in ');
   }

   // decrypt symmetric key
   $encryption_key = base64_decode($encoded_message->{'sec:encryptionKey'});
   if(openssl_private_decrypt($encryption_key, $skey, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the encryption key.');
   }

   // decrypt IV
   $encIv = base64_decode($encoded_message->{'sec:iv'});
   if(openssl_private_decrypt($encIv, $iv, $pkey) === false)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the initialization vector (IV).');
   }

   // free private key resource
   openssl_pkey_free($pkey);

   // decrypt data
   $data = openssl_decrypt(
      $encoded_message->{'sec:data'}, 'aes128', $skey, false, $iv);

   if(!$data)
   {
      throw new Exception('PaySwarm Decryption Exception:' .
         'Failed to decrypt the encrypted message due to an incorrect ' .
         'symmetric key or an invalid initialization vector.');
   }

   // decode JSON message
   return json_decode($data);
}

// TODO: methods to use CURL to GET/POST
// TODO: method to fetch public key IRI and check for PA ownership based on
// PA IRI from config

?>
