<?php

require_once('payswarm-session.inc');

/**
 * Defines the PaySwarm Session widget.
 *
 * @package payswarm
 * @since 1.0
 */
class PaySwarmSessionWidget extends WP_Widget {
  function PaySwarmSessionWidget() {
    parent::WP_Widget('payswarm_session', 'PaySwarm Session', array(
      'classname' => 'PaySwarmSessionWidget',
      'description' => 'Indicates whether a PaySwarm session is active.'
    ));
  }

  /**
   * Renders the widget to the screen.
   *
   * @param assoc $args the widget arguments.
   * @param WP_Widget $instance the widget instance.
   */
  function widget($args, $instance) {
    extract($args);

    $session = payswarm_get_session(null, $return_expired=true);
    $pslogo_url = PAYSWARM_PLUGIN_URL . 'images/payswarm-20.png';
    if(is_array($session) && $session['identity_id'] !== null) {
      $url = plugins_url() . '/payswarm/payswarm-logout.php?redirect=' .
        urlencode($_SERVER['REQUEST_URI']);
      echo $before_widget;
      ?>
      <span class="payswarm-session"
        title="Logged into PaySwarm as '<?php echo $session['identity_id']; ?>'">
        <img src="<?php echo $pslogo_url; ?>" />
          Logged into PaySwarm
        <span class="payswarm-menu">
          <span class="payswarm-menu-item">
            <a href="<?php echo $session['identity_id']; ?>">My Identity</a>
          </span>
          <span class="payswarm-menu-item">
            <a href="<?php echo $url; ?>">Logout</a>
          </span>
        </span>
      </span>
      <?php
      echo $after_widget;
    }
    // session expired
    else if(is_numeric($session)) {
      echo $before_widget;
      echo "<span class=\"payswarm-session\"><img src=\"$pslogo_url\" /> You've been logged out</span>";
      echo $after_widget;
    }/*
    else {
      echo $before_widget;
      echo "<span class=\"payswarm-session\"><img src=\"$pslogo_url\" /> Not logged into PaySwarm</span>";
      echo $after_widget;
    }*/
  }

  /**
   * Called when a user clicks 'Save' on the settings page on the widget
   * administration screen.
   *
   * @param WP_Widget $new_instance the new widget instance.
   * @param WP_Widget $old_instance the old widget instance.
   *
   * @return WP_Widget the new widget instance.
   */
  function update($new_instance, $old_instance) {
    return $new_instance;
  }

  /**
   * Displays the settings page for the widget on the widget administration
   * screen.
   *
   * @param WP_Widget $instance the widget instance.
   */
  /*function form($instance) {
  }*/
}

// register widgets
add_action('widgets_init', 'payswarm_init_widgets');
function payswarm_init_widgets() {
  register_widget('PaySwarmSessionWidget');
}

// install default widgets on activation
function payswarm_install_widgets() {
  // add widget by default if no widget settings are present
  $settings = get_option('widget_payswarm_session', false);
  if($settings === false) {
    // manually create widget settings
    $settings = array(2 => array(), '_multiwidget' => 1);
    update_option('widget_payswarm_session', $settings);

    // add widget to beginning of primary-widget-area
    $sidebars_widgets = wp_get_sidebars_widgets();
    array_splice(
      $sidebars_widgets['primary-widget-area'], 0, 0,
      'payswarm_session-2');
    wp_set_sidebars_widgets($sidebars_widgets);
  }
}

/* end of file, omit ?> */
