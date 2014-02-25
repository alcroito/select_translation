<?php
class select_translation_filter_handler extends views_handler_filter {
  // Don't display empty space where the operator would be.
  var $no_operator = TRUE;
  
  function option_definition() {
    $options = parent::option_definition();

    $options['value']['default'] = 'default';
    $options['priorities']['default'] = '';

    return $options;
  }

  function operator_form(&$form, &$form_state) {
    $form['operator'] = array();
  }
  
  function value_form(&$form, &$form_state) {
    $form['value'] = array(
      '#type' => 'radios',
      '#title' => t('Select translation selection mode'),
      '#options' => array(
        'original' => t('Use current language ; if not available use original language'),
        'default'  => t('Use current language ; if not available use default language ; if not available use original language'),
        'list'     => t('Choose language priorities below:'),
       ),
      '#default_value' => $this->value,
    );
    $form['priorities'] = array(
      '#type' => 'textfield',
      '#title' => t('Language priorities'),
      '#description' => t('If selection mode is set to "Choose language priorities", 
                           you can enter here a comma separated list of language codes. 
                           The filter will then return the node in the first available langauge
                           in that list ; and the original version if no match were found.<br/><br/>
                           The special value "current" will be replaced with the current language,
                           "default" will be replaced witht the default language, 
                           and "original" with the original language of each node.
                           <br/><br/>
                           Example:<br/><em>en,fr,current,default,original</em><br/>
                           This will return the version in english if available, if not in french,
                           if not in the current language, if not in the default language.
                           If none are available it will return the original version.'),
      '#default_value' => !empty($this->options['priorities']) ? $this->options['priorities'] : '',
    );
  }

  function query() {
    $default_language = language_default('language');
    global $language_content;
    $current_language = $language_content->language;

    // Prepare input
    $list = array();
    if ($this->value == 'original') {
      $list = array('current');
    }
    elseif ($this->value == 'default') {
      $list = array('current', 'default');
    }
    elseif (!empty($this->options['priorities'])) {
      $list = explode(',', $this->options['priorities']);
      foreach ($list as $i => $v) {
        $list[$i] = strtolower(trim($v));
      }
    }
    $list[] = 'original';
    foreach ($list as $i => $v) {
      if ($list[$i] == 'current') {
        $list[$i] = $current_language;
      }
      elseif ($list[$i] == 'default') {
        $list[$i] = $default_language;
      }
    }
    $list = array_unique($list);

    // Now build the query.
    $alias = $this->query->ensure_table('node');
    $condition_holder = db_or();

    // First include nodes that don't have translations.
    $condition_holder->condition("$alias.tnid", 0);

    $i = 0;
    $exclude = array();
    // Now go through each language.
    foreach ($list as $language) {
      if ($language != 'original') {
        // Create a views join on the node table, and add it as a relationship.
        // This is used to find if there are translations of a certain language.
        $join = new views_join();
        $sub_query_alias = 'nt' . $i;
        $extra = "$sub_query_alias.language = '$language'";
        $join->construct('node', $alias, 'nid', 'tnid', $extra);
        $this->query->add_relationship($sub_query_alias, $join);
      }

      if ($language == 'original') {
        // Include nodes that are the base of a translation (aka original).
        $and = db_and()->where("$alias.nid = $alias.tnid");
      }
      else {
        // Include nodes of specified language.
        $and = db_and()->condition("$alias.language", $language);
      }

      // Add the currently processed language only if previous languages were not found.
      foreach ($exclude as $e) {
        $and->where($e);
      }
      $condition_holder->condition($and);

      // Add to excludes the currently processed translation language.
      if (isset($sub_query_alias)) {
        $exclude[] = $sub_query_alias . '.nid IS NULL';
      }
      $i++;
    }

    // Add in the conditions.
    $this->query->add_where($this->options['group'], $condition_holder);
  }
}
