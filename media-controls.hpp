#pragma once

#include <QTimer>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QVector>
#include <QStringList>
#include <obs.hpp>
#include <QDockWidget>
#include <memory>

#include <obs-frontend-api.h>

#include "media-control.hpp"
#include "ui_MediaControls.h"

class MediaControls : public QWidget {
	Q_OBJECT

private:
	static void OBSSignal(void *data, const char *signal, calldata_t *calldata);
	static void AddActiveSource(obs_source_t *parent, obs_source_t *child, void *param);
	static bool AddSource(void *param, obs_source_t *source);
	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);
	void ControlContextMenu();
	void ToggleShowTimeDecimals();
	void ToggleShowTimeRemaining();
	void ToggleAllSources();
	void RefreshMediaControls();
	void HideMenu(QMenu *menu);
	void SetupMusicLibrary();
	void AddMusicFiles(const QStringList &paths);
	void UpdateLibraryFilter(const QString &query);
	void SaveMusicLibrary();
	void LoadMusicLibrary();
	void RefreshVlcSources();
	void PlayLibraryRow(int row);
	void SendPlaylistToVlc(int startRow);

	void addMediaControl(obs_source_t *source, int column);

	QTableWidget *libraryTable = nullptr;
	QLineEdit *librarySearch = nullptr;
	QLabel *libraryStatus = nullptr;
	QComboBox *vlcSources = nullptr;
	QStringList musicFiles;
	bool showTimeDecimals = false;
	bool showTimeRemaining = false;
	bool allSources = false;
	std::unique_ptr<Ui::MediaControls> ui;

private slots:
	void SignalMediaSource();

public:
	MediaControls(QWidget *parent = nullptr);
	~MediaControls();
};
