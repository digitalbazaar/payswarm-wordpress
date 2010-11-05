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
 * @global string PAYSWARM_TABLE_NAME
 */
define('PAYSWARM_TABLE_NAME', $wpdb->prefix . 'payswarm_tokens');

/**
 * Installs/upgrades the PaySwarm database into a WordPress installation.
 * 
 * @package payswarm
 * @since 1.0
 */
function payswarm_install_database()
{
   global $wpdb;

   // check to see if the payswarm table exists
   if($wpdb->get_var('SHOW TABLES LIKE \''. PAYSWARM_TABLE_NAME .'\'') != 
      PAYSWARM_TABLE_NAME)
   {
      // ensure that the formatting below is one field per line, dbDelta is
      // very picky when it comes to what it allows.
      $sql = 'CREATE TABLE ' . PAYSWARM_TABLE_NAME . ' (
         session VARCHAR(255) NOT NULL, 
         state TINYTEXT, 
         token TINYTEXT,
         secret TINYTEXT, 
         amount TINYTEXT, 
         mtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP, 
         PRIMARY KEY (session)
         );';

      require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
      dbDelta($sql);
      update_option("payswarm_schema_version", PAYSWARM_SCHEMA_VERSION);
   }

   // FIXME: We need to put in code to upgrade the schema if we ever change
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
 * @param string $payswarm_session the PaySwarm session identifier.
 * @return Array An associative array of all items associated with the given
 *    PaySwarm session or NULL if no session exists.
 */
function payswarm_database_get_token($payswarm_session, $create=false)
{
   $rval = null;
   global $wpdb;

   // create the token if it doesn't already exist in the database
   if($create)
   {
      $wpdb->query($wpdb->prepare('INSERT IGNORE INTO ' . PAYSWARM_TABLE_NAME . 
         '(session, state, token, secret, amount) ' .
         'VALUES (%s, %s, %s, %s, %s)',
         $payswarm_session, 'initializing', '', '', '0.0'));
   }

   // retrieve all information associated with the session
   $rval = $wpdb->get_row($wpdb->prepare(
      'SELECT session, state, token, secret, amount FROM ' .
      PAYSWARM_TABLE_NAME . ' WHERE session=%s', $payswarm_session),
      ARRAY_A);

   // set the return type to null if there was nothing in the database
   if($wpdb->num_rows < 1)
   {
      $rval = null;
   }

   return $rval;
}

/**
 * Updates a PaySwarm Payment Token given information associated with the token.
 * 
 * Updates all of the token information that is provided. The token contains 
 * information such as session, state, the token identifier, token secret, 
 * and the amount left on the token.
 *
 * @package payswarm
 * @since 1.0
 *
 * @param Array $payswarm_token An associative array containing all of the 
 *    values for a $payswarm_token that should be updated. 
 * @return boolean true if the update was successful, false otherwise.
 */
function payswarm_database_update_token($payswarm_token)
{
   $rval = false;
   global $wpdb;

   // build the update data in WordPress database update format
   $data = array();
   foreach($payswarm_token as $key => $value)
   {
      $data[$key] = $value;
   }

   // retrieve all information associated with the session
   $wpdb->update(PAYSWARM_TABLE_NAME, $data, 
      array("session" => $payswarm_token["session"]));

   // Set the return type to true if the object was updated correctly
   if($wpdb->num_rows > 0)
   {
      $rval = true;
   }

   return $rval;
}
?>
