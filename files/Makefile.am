install-data-hook:
	find -mindepth 1 -maxdepth 1 -type d -exec cp -R {} $(DESTDIR)/ \;

uninstall-hook:
	find -mindepth 1 -type f -exec rm $(DESTDIR)/{} \;
