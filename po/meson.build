po_subdir = meson.current_source_dir()
# This function also defines targets for maintainers to use: Note: These output to the source directory
# <project_id>-pot: runs xgettext to regenerate the pot file
# <project_id>-update-po: regenerates the .po files from current .pot file
# <project_id>-gmo: builds the translations without installing
i18n.gettext(gettext_package,
  install: true,
)
