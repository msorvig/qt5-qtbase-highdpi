TEMPLATE = subdirs

SUBDIRS *= sqldrivers bearer
qtHaveModule(gui): SUBDIRS *= imageformats platforms platforminputcontexts platformthemes generic
qtHaveModule(widgets): SUBDIRS += accessible

!wince*:qtHaveModule(widgets): SUBDIRS += printsupport
