desktop_gschemas_in_in =						\
	org.cinnamon.desktop.default-applications.gschema.xml.in.in	\
	org.cinnamon.desktop.interface.gschema.xml.in.in			\
	org.cinnamon.desktop.lockdown.gschema.xml.in.in			\
	org.cinnamon.desktop.sound.gschema.xml.in.in			\
	org.cinnamon.desktop.thumbnail-cache.gschema.xml.in.in		\
	org.cinnamon.desktop.a11y.keyboard.gschema.xml.in.in		\
	org.cinnamon.desktop.a11y.applications.gschema.xml.in.in		\
	org.cinnamon.desktop.a11y.magnifier.gschema.xml.in.in		\
	org.cinnamon.desktop.a11y.mouse.gschema.xml.in.in			\
	org.cinnamon.desktop.thumbnailers.gschema.xml.in.in		\
	org.cinnamon.desktop.session.gschema.xml.in.in			\
	org.cinnamon.desktop.background.gschema.xml.in.in			\
	org.cinnamon.desktop.media-handling.gschema.xml.in.in		\
	org.cinnamon.desktop.screensaver.gschema.xml.in.in			\
	org.cinnamon.desktop.wm.preferences.gschema.xml.in.in		\
	org.cinnamon.desktop.notifications.gschema.xml.in.in		\
	org.cinnamon.desktop.input-sources.gschema.xml.in.in		\
	org.cinnamon.desktop.keybindings.gschema.xml.in.in		\
	org.cinnamon.desktop.keybindings.media-keys.gschema.xml.in.in		\
	org.cinnamon.desktop.keybindings.wm.gschema.xml.in.in    \
	org.cinnamon.desktop.privacy.gschema.xml.in.in

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_ENUM_NAMESPACE = org.cinnamon.desktop
gsettings_ENUM_FILES = $(top_srcdir)/libcinnamon-desktop/*.h

desktop_gschemas_in = $(desktop_gschemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS = $(desktop_gschemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@datadir\@^$(datadir)^g' -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

EXTRA_DIST =				\
	$(desktop_gschemas_in_in)

CLEANFILES =					\
	$(gsettings__enum_file)			\
	$(gsettings_SCHEMAS)			\
	$(desktop_gschemas_in)			\
	*.gschema.valid

-include $(top_srcdir)/git.mk
