
#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QAction>

struct BrushexportDialog
{
	QWidget *window;
	QPushButton *b_export;
	QListWidget *t_materialist;

	QRadioButton *r_collapse;
	QRadioButton *r_collapsebymaterial;
	QRadioButton *r_nocollapse;

	QCheckBox *t_exportmaterials;
	QCheckBox *t_limitmatnames;
	QCheckBox *t_objects;
	QCheckBox *t_weld;

	QLineEdit *ed_materialname;
};

inline BrushexportDialog g_dialog{};


namespace callbacks {

void OnExportClicked( bool choose_path );
void OnAddMaterial();
void OnRemoveMaterial();

} // callbacks
