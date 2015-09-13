ZBase.registerView((function() {

  var render = function(path) {

    var page = $.getTemplate(
        "views/documentation",
        "documentation_main_tpl");

    var menu = DocumentationMenu();
    menu.render($(".zbase_documentation_menu_sidebar", page));
    $.handleLinks(page);
    $.replaceViewport(page);

    var content = $(".zbase_documentation_content");

    if(path) {
      var key = path.split("/").pop();
      $.httpGet("/a/_/d/" + key, function(res) {
        //content.innerHTML = res.response;
        content.innerHTML = key;
      });
    }

    $.hideLoader();
  };

  return {
    name: "documentation",
    loadView: function(params) { render(); },
    unloadView: function() {},
    handleNavigationChange: render
  };

})());