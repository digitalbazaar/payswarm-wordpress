<?php
global $wpdb;
/**
 * The current database schema version.
 * @since 1.0
 * @global string PAYSWARM_SCHEMA_VERSION
 */
define('PAYSWARM_SCHEMA_VERSION', '1.0');

/**
 * The name of the table where PaySwarm stores session/token information.
 * @since 1.0
 * @global string PAYSWARM_TOKENS_TABLE_NAME
 */
define('PAYSWARM_TOKENS_TABLE_NAME', $wpdb->prefix . 'payswarm_tokens');

/**
 * The name of the table where PaySwarm stores article information.
 * @since 1.0
 * @global string PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME
 */
define('PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME',
   $wpdb->prefix . 'payswarm_authorized_posts');

/**
 * Installs/upgrades the PaySwarm database into a WordPress installation.
 *
 * @package payswarm
 * @since 1.0
 */
function payswarm_install_database()
{
   global $wpdb;

   // check to see if the payswarm tokens and authorized posts tables exist
   if(($wpdb->get_var(
         'SHOW TABLES LIKE \''. PAYSWARM_TOKENS_TABLE_NAME .'\'') !==
         PAYSWARM_TOKENS_TABLE_NAME) ||
      ($wpdb->get_var(
         'SHOW TABLES LIKE \''. PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME .'\'') !==
         PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME))
   {
      // ensure that the formatting below is one field per line, dbDelta is
      // very picky when it comes to what it allows.
      $tokens_sql = 'CREATE TABLE ' . PAYSWARM_TOKENS_TABLE_NAME . ' (
         session VARCHAR(40) NOT NULL,
         ip VARCHAR(15) NOT NULL,
         scope TINYTEXT,
         state TINYTEXT,
         token TINYTEXT,
         secret TINYTEXT,
         details TEXT,
         mtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
         PRIMARY KEY (session,ip)
         );';

      $posts_sql = 'CREATE TABLE ' . PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME . ' (
         customer_id VARCHAR(255) NOT NULL,
         post_id INTEGER,
         INDEX (customer_id)
         );';

      require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
      dbDelta($tokens_sql);
      dbDelta($posts_sql);
      update_option("payswarm_schema_version", PAYSWARM_SCHEMA_VERSION);
   }

   // NOTE: We need to put in code here to upgrade the schema if we ever change
   // the database schema
}

/**
 * Retrieves the token associated with a PaySwarm session.
 *
 * Retrieves all of the token information that is associated with a PaySwarm
 * session. The token contains information such as session, state, the token
 * identifier, token secret, and the amount left on the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param string $session the PaySwarm session identifier.
 * @param string $scope the intended scope of the PaySwarm session identifier.
 * @param string $create true if the token should be created.
 *
 * @return Array An associative array of all items associated with the given
 *    PaySwarm session or NULL if no session exists.
 */
function payswarm_database_get_token(
   $session, $scope, $create = false)
{
   $rval = null;
   global $wpdb;

   // get client IP address
   $ip = $_SERVER['REMOTE_ADDR'];

   // create the token if it doesn't already exist in the database
   if($create)
   {
      $wpdb->query($wpdb->prepare(
         'INSERT IGNORE INTO ' . PAYSWARM_TOKENS_TABLE_NAME .
         '(session, ip, scope, state, token, secret, details) ' .
         'VALUES (%s, %s, %s, %s, %s, %s, %s)',
         $session, $ip, $scope, 'initializing', '', '',
         '{}'));
   }

   // retrieve all information associated with the session
   $rval = $wpdb->get_row($wpdb->prepare(
      'SELECT session, scope, state, token, secret, details, mtime FROM ' .
      PAYSWARM_TOKENS_TABLE_NAME .
      ' WHERE session=%s AND ip=%s AND scope=%s LIMIT 1',
      $session, $ip, $scope), ARRAY_A);

   // set the return type to null if there was nothing in the database
   if($wpdb->num_rows < 1)
   {
      $rval = null;
   }
   else
   {
      // decode details
      $rval['details'] = json_decode($rval['details'], true);
   }

   return $rval;
}

/**
 * Updates a PaySwarm Token given information associated with the token.
 *
 * Updates all of the token information that is provided. The token contains
 * information such as session, state, the token identifier, token secret,
 * and the amount left on the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $token an associative array containing all of the
 *    values for a $token that should be updated.
 *
 * @return boolean true if the update was successful, false otherwise.
 */
function payswarm_database_update_token($token)
{
   global $wpdb;

   // json encode details
   $token['details'] = json_encode($token['details']);

   // update all information associated with the session
   $rval = $wpdb->update(PAYSWARM_TOKENS_TABLE_NAME, $token,
      array('session' => $token['session'], 'scope' => $token['scope']));

   // set the return type to true if the object was updated correctly
   return ($rval !== false);
}

/**
 * Deletes a PaySwarm Token given information associated with the token.
 *
 * The information given must at least contain the session and scope associated
 * with the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token An associative array containing at least
 *    the session and scope for the token that should be deleted.
 *
 * @return boolean true if the delete was successful, false otherwise.
 */
function payswarm_database_delete_token($payswarm_token)
{
   $rval = false;
   global $wpdb;

   // delete all information associated with the session
   $dtoken = 'DELETE FROM ' . PAYSWARM_TOKENS_TABLE_NAME .
      " WHERE session='%s' AND scope = '%s'";
   $wpdb->query($wpdb->prepare(
      $dtoken, $payswarm_token['session'], $payswarm_token['scope']));

   // set the return type to true if one or more objects were deleted
   if($wpdb->num_rows > 0)
   {
      $rval = true;
   }

   return $rval;
}

/**
 * Checks to see if a post is authorized using the given token. A post is
 * authorized if the customer ID in the token has been granted access to
 * view the post.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token an associative array containing the token info.
 * @param Integer $post_id the post identifier.
 *
 * @return boolean true if the post is authorized, false if not.
 */
function payswarm_database_is_post_authorized($payswarm_token, $post_id)
{
   $rval = false;
   global $wpdb;

   // see if a customer ID is set on the token
   if(isset($payswarm_token['details']['customer_id']))
   {
      $customer_id = $payswarm_token['details']['customer_id'];
      $rval = $wpdb->get_row($wpdb->prepare(
            'SELECT post_id FROM ' .
            PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME .
            ' WHERE customer_id=%s and post_id=%s LIMIT 1',
            $customer_id, $post_id), ARRAY_A);
      $rval = (is_array($rval) && count($rval) === 1);
   }

   return $rval;
}

/**
 * Authorizes a post to be viewed for a particular customer ID. If the given
 * token does not contain a customer ID, the post will not be authorized.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token an associative array containing the token info.
 * @param Integer $post_id the post identifier.
 *
 * @return boolean true if the post was authorized, false if not.
 */
function payswarm_database_authorize_post($payswarm_token, $post_id)
{
   $rval = false;
   global $wpdb;

   // see if a customer ID is set on the token
   if(isset($payswarm_token['details']['customer_id']))
   {
      $customer_id = $payswarm_token['details']['customer_id'];
      $rval = $wpdb->query($wpdb->prepare(
         'INSERT IGNORE INTO ' . PAYSWARM_AUTHORIZED_POSTS_TABLE_NAME . ' ' .
         '(customer_id, post_id) VALUES (%s, %s)',
         $customer_id, $post_id));
      $rval = ($rval !== false);
   }

   return $rval;
}

/**
 * Deletes old post metadata from a post given a post ID and metadata ID and
 * name.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Integer $post_id the post identifier.
 * @param Integer $meta_id the metadata identifier.
 * @param String $meta_key the metadata key value.
 */
function payswarm_database_remove_old_metadata($post_id, $meta_id, $meta_key)
{
   global $wpdb;

   // delete the post identifier
   $wpdb->query($wpdb->prepare(
      'DELETE FROM ' . $wpdb->postmeta . ' ' .
      'WHERE post_id=%s AND meta_id<%s AND meta_key=%s',
      $post_id, $meta_id, $meta_key));
}

?>
