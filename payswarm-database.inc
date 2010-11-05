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
function payswarm_database_get_token($payswarm_session)
{
   $rval = null;
   global $wpdb;
   global $payswarm_table_name;

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
?>
