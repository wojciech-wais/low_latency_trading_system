// Mobile menu toggle
document.addEventListener('DOMContentLoaded', function() {
  var toggle = document.querySelector('.menu-toggle');
  var navLinks = document.querySelector('.nav-links');

  if (toggle && navLinks) {
    toggle.addEventListener('click', function() {
      navLinks.classList.toggle('open');
    });

    // Close menu when clicking a link
    navLinks.querySelectorAll('a').forEach(function(link) {
      link.addEventListener('click', function() {
        navLinks.classList.remove('open');
      });
    });
  }
});
