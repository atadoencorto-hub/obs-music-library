#include "media-controls.hpp"
#include <QToolTip>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QComboBox>
#include <QSignalBlocker>
#include <QUrl>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

#include <obs-module.h>
#include <QMainWindow>
#include <QMenuBar>
#include <QVBoxLayout>
#include "ui_MediaControls.h"

#include "media-control.hpp"
#include "version.h"
#include "util/platform.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("media-controls", "en-US")

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

bool obs_module_load()
{
	blog(LOG_INFO, "[Media Controls] loaded version %s", PROJECT_VERSION);
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);
	auto *tmp = new MediaControls(main_window);

	const QString title = QString::fromUtf8(obs_module_text("MediaControls"));
	const auto name = "MediaControls";
	obs_frontend_add_dock_by_id(name, title.toUtf8().constData(), tmp);
	obs_frontend_pop_ui_translation();
	return true;
}

void obs_module_unload() {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("MediaControls");
}

void MediaControls::OBSSignal(void *data, const char *signal, calldata_t *call_data)
{
	UNUSED_PARAMETER(signal);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(call_data, "source"));
	if (!source)
		return;
	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_CONTROLLABLE_MEDIA) == 0)
		return;

	MediaControls *controls = static_cast<MediaControls *>(data);
	QMetaObject::invokeMethod(controls, "SignalMediaSource", Qt::QueuedConnection);
}

void MediaControls::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	MediaControls *controls = reinterpret_cast<MediaControls *>(ptr);

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		controls->RefreshMediaControls();
		break;
	default:
		break;
	}
}

void MediaControls::AddActiveSource(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	if (obs_source_removed(child))
		return;
	auto *controls = static_cast<MediaControls *>(param);
	const uint32_t flags = obs_source_get_output_flags(child);
	if ((flags & OBS_SOURCE_CONTROLLABLE_MEDIA) == 0) {
		obs_source_enum_active_sources(child, AddActiveSource, param);
		return;
	}

	obs_data_t *priv_settings = obs_source_get_private_settings(child);
	bool hidden = obs_data_get_bool(priv_settings, "media_hidden");
	obs_data_release(priv_settings);
	if (hidden)
		return;

	QString source_name = QT_UTF8(obs_source_get_name(child));
	const auto count = controls->ui->verticalLayout->count();
	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = controls->ui->verticalLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = dynamic_cast<MediaControl *>(item->widget());
		if (w->objectName() == source_name || w->GetSource() == OBSGetWeakRef(child)) {
			return;
		}
	}

	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = controls->ui->verticalLayout->itemAt(i);
		if (item) {
			QWidget *w = item->widget();
			if (source_name.localeAwareCompare(w->objectName()) >= 0) {
				controls->addMediaControl(child, i + 1);
				return;
			}
		}
	}
	controls->addMediaControl(child, 0);
}

void MediaControls::addMediaControl(obs_source_t *source, int column)
{
	MediaControl *c = new MediaControl(OBSGetWeakRef(source), showTimeDecimals, showTimeRemaining);
	ui->verticalLayout->insertWidget(column, c);
}

bool MediaControls::AddSource(void *param, obs_source_t *source)
{
	if (obs_source_removed(source))
		return true;
	MediaControls *controls = static_cast<MediaControls *>(param);
	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_CONTROLLABLE_MEDIA) == 0)
		return true;

	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	bool hidden = obs_data_get_bool(priv_settings, "media_hidden");
	obs_data_release(priv_settings);
	if (hidden)
		return true;

	QString source_name = QT_UTF8(obs_source_get_name(source));
	const auto count = controls->ui->verticalLayout->count();
	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = controls->ui->verticalLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = dynamic_cast<MediaControl *>(item->widget());
		if (w->objectName() == source_name || w->GetSource() == OBSGetWeakRef(source)) {
			return true;
		}
	}
	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = controls->ui->verticalLayout->itemAt(i);
		if (item) {
			QWidget *w = item->widget();
			if (source_name.localeAwareCompare(w->objectName()) >= 0) {
				controls->addMediaControl(source, i + 1);
				return true;
			}
		}
	}
	controls->addMediaControl(source, 0);
	return true;
}

MediaControls::MediaControls(QWidget *parent) : QWidget(parent), ui(new Ui::MediaControls)
{
	ui->setupUi(this);
	SetupMusicLibrary();
	LoadMusicLibrary();

	char *file = obs_module_config_path("config.json");
	obs_data_t *data = nullptr;
	if (file) {
		data = obs_data_create_from_json_file(file);
		bfree(file);
	}
	if (data) {
		showTimeDecimals = obs_data_get_bool(data, "showTimeDecimals");
		showTimeRemaining = obs_data_get_bool(data, "showTimeRemaining");
		allSources = obs_data_get_bool(data, "showAllSources");
		obs_data_release(data);
	}

	connect(this, &QWidget::customContextMenuRequested, this, &MediaControls::ControlContextMenu);

	signal_handler_connect_global(obs_get_signal_handler(), OBSSignal, this);
	obs_frontend_add_event_callback(OBSFrontendEvent, this);

	hide();
}

MediaControls::~MediaControls()
{
	SaveMusicLibrary();
	signal_handler_disconnect_global(obs_get_signal_handler(), OBSSignal, this);
	char *file = obs_module_config_path("config.json");
	if (!file) {
		deleteLater();
		return;
	}
	obs_data_t *data = obs_data_create_from_json_file(file);
	if (!data)
		data = obs_data_create();
	obs_data_set_bool(data, "showTimeDecimals", showTimeDecimals);
	obs_data_set_bool(data, "showTimeRemaining", showTimeRemaining);
	obs_data_set_bool(data, "showAllSources", allSources);
	if (!obs_data_save_json(data, file)) {
		char *path = obs_module_config_path("");
		if (path) {
			os_mkdirs(path);
			bfree(path);
		}
		obs_data_save_json(data, file);
	}
	obs_data_release(data);
	bfree(file);
	deleteLater();
}

void MediaControls::SignalMediaSource()
{
	RefreshMediaControls();
}

void MediaControls::ControlContextMenu()
{

	QAction showTimeDecimalsAction(obs_module_text("ShowTimeDecimals"), this);
	showTimeDecimalsAction.setCheckable(true);
	showTimeDecimalsAction.setChecked(showTimeDecimals);

	QAction showTimeRemainingAction(obs_module_text("ShowTimeRemaining"), this);
	showTimeRemainingAction.setCheckable(true);
	showTimeRemainingAction.setChecked(showTimeRemaining);

	QAction allSourcesAction(obs_module_text("AllSources"), this);
	allSourcesAction.setCheckable(true);
	allSourcesAction.setChecked(allSources);

	connect(&showTimeDecimalsAction, &QAction::toggled, this, &MediaControls::ToggleShowTimeDecimals, Qt::DirectConnection);

	connect(&showTimeRemainingAction, &QAction::toggled, this, &MediaControls::ToggleShowTimeRemaining, Qt::DirectConnection);

	connect(&allSourcesAction, &QAction::toggled, this, &MediaControls::ToggleAllSources, Qt::DirectConnection);

	QMenu popup;
	popup.addAction(&showTimeDecimalsAction);
	popup.addAction(&showTimeRemainingAction);
	popup.addSeparator();
	popup.addAction(&allSourcesAction);
	popup.addSeparator();
	auto t = popup.addMenu(obs_module_text("Hide"));
	connect(t, &QMenu::aboutToShow, [this, t] { HideMenu(t); });

	popup.exec(QCursor::pos());
}

void MediaControls::ToggleShowTimeDecimals()
{
	showTimeDecimals = !showTimeDecimals;
	RefreshMediaControls();
}

void MediaControls::ToggleShowTimeRemaining()
{
	showTimeRemaining = !showTimeRemaining;
	RefreshMediaControls();
}

void MediaControls::ToggleAllSources()
{
	allSources = !allSources;
	RefreshMediaControls();
}

void MediaControls::RefreshMediaControls()
{
	const auto count = ui->verticalLayout->count();
	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = ui->verticalLayout->itemAt(i);
		if (!item)
			continue;
		QWidget *w = item->widget();
		obs_source_t *source = obs_get_source_by_name(QT_TO_UTF8(w->objectName()));
		if (!source) {
			ui->verticalLayout->removeItem(item);
			MediaControl::OBSRemove(w, nullptr);
			w->deleteLater();
		} else {
			if (obs_source_removed(source) || (!allSources && !obs_source_active(source))) {
				ui->verticalLayout->removeItem(item);
				MediaControl::OBSRemove(w, nullptr);
				w->deleteLater();
			} else {
				obs_data_t *priv_settings = obs_source_get_private_settings(source);
				bool hidden = obs_data_get_bool(priv_settings, "media_hidden");
				obs_data_release(priv_settings);
				if (hidden) {
					ui->verticalLayout->removeItem(item);
					MediaControl::OBSRemove(w, nullptr);
					w->deleteLater();
				} else {
					MediaControl *mc = (MediaControl *)w;
					mc->showTimeDecimals = showTimeDecimals;
					mc->showTimeRemaining = showTimeRemaining;
				}
			}
			obs_source_release(source);
		}
	}

	if (allSources) {
		obs_enum_sources(AddSource, this);
	} else {
		obs_source_t *scene = obs_frontend_get_current_scene();
		if (scene) {
			obs_source_enum_active_sources(scene, AddActiveSource, this);
			obs_source_release(scene);
		}
		scene = obs_frontend_get_current_preview_scene();
		if (scene) {
			obs_source_enum_active_sources(scene, AddActiveSource, this);
			obs_source_release(scene);
		}
	}
}

void MediaControls::HideMenu(QMenu *menu)
{
	menu->clear();
	std::pair<MediaControls *, QMenu *> data(this, menu);
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto t = (std::pair<MediaControls *, QMenu *> *)data;
			QMenu *menu = t->second;
			if (obs_source_removed(source))
				return true;
			uint32_t flags = obs_source_get_output_flags(source);
			if ((flags & OBS_SOURCE_CONTROLLABLE_MEDIA) == 0)
				return true;

			obs_data_t *priv_settings = obs_source_get_private_settings(source);
			bool hidden = obs_data_get_bool(priv_settings, "media_hidden");
			obs_data_release(priv_settings);

			QString source_name = QT_UTF8(obs_source_get_name(source));

			QList<QAction *> actions = menu->actions();
			QAction *before = nullptr;
			for (QAction *menuAction : actions) {
				if (menuAction->text().compare(source_name) >= 0)
					before = menuAction;
			}
			auto action = new QAction(source_name, menu);
			menu->insertAction(before, action);
			action->setCheckable(true);
			action->setChecked(hidden);
			auto mc = t->first;
			connect(action, &QAction::triggered, [mc, source] {
				obs_data_t *priv_settings = obs_source_get_private_settings(source);
				bool hidden = obs_data_get_bool(priv_settings, "media_hidden");
				obs_data_set_bool(priv_settings, "media_hidden", !hidden);
				obs_data_release(priv_settings);
				mc->RefreshMediaControls();
			});
			return true;
		},
		&data);
}


// The library is intentionally independent from the OBS source controls. It
// reads metadata from local files and never replaces Exeldro's seek handlers.
void MediaControls::SetupMusicLibrary()
{
 auto *panel = new QWidget(this);
 auto *layout = new QVBoxLayout(panel);
 layout->setContentsMargins(5, 4, 5, 4);
 layout->setSpacing(3);
 auto *toolbar = new QHBoxLayout();
 toolbar->setSpacing(4);
 auto *heading = new QLabel(tr("BIBLIOTECA MUSICAL"), panel);
 heading->setStyleSheet("font-weight:600");
 toolbar->addWidget(heading);
 vlcSources = new QComboBox(panel);
 vlcSources->setMinimumWidth(100);
 vlcSources->setToolTip(tr("Fuente VLC de OBS que reproducirá la biblioteca"));
 toolbar->addWidget(vlcSources);
 auto *refreshVlc = new QPushButton(tr("↻"), panel);
 refreshVlc->setToolTip(tr("Actualizar fuentes VLC"));
 toolbar->addWidget(refreshVlc);
 connect(refreshVlc, &QPushButton::clicked, this, &MediaControls::RefreshVlcSources);
 librarySearch = new QLineEdit(panel);
 librarySearch->setPlaceholderText(tr("Buscar título, artista, álbum o año"));
 librarySearch->setClearButtonEnabled(true);
 toolbar->addWidget(librarySearch, 1);
 auto *add = new QPushButton(tr("+ Archivos"), panel);
 auto *folder = new QPushButton(tr("+ Carpeta"), panel);
 auto *remove = new QPushButton(tr("Eliminar"), panel);
 toolbar->addWidget(add);
 toolbar->addWidget(folder);
 toolbar->addWidget(remove);
 layout->addLayout(toolbar);
 libraryTable = new QTableWidget(panel);
 libraryTable->setColumnCount(5);
 libraryTable->setHorizontalHeaderLabels({tr("Título"), tr("Artista"), tr("Álbum"), tr("Año"), tr("Duración")});
 libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
 libraryTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
 libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
 libraryTable->setAlternatingRowColors(true);
 libraryTable->setSortingEnabled(true);
 libraryTable->setMinimumHeight(100);
 libraryTable->setMaximumHeight(210);
 libraryTable->verticalHeader()->hide();
 libraryTable->verticalHeader()->setDefaultSectionSize(21);
 libraryTable->horizontalHeader()->setStretchLastSection(false);
 libraryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
 libraryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
 libraryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
 libraryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
 libraryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
 layout->addWidget(libraryTable);
 libraryStatus = new QLabel(tr("0 canciones · Doble clic: reproducir en VLC"), panel);
 layout->addWidget(libraryStatus);
 ui->verticalLayout_1->addWidget(panel);
 RefreshVlcSources();
 connect(librarySearch, &QLineEdit::textChanged, this, &MediaControls::UpdateLibraryFilter);
 connect(add, &QPushButton::clicked, this, [this]() {
   AddMusicFiles(QFileDialog::getOpenFileNames(this, tr("Añadir canciones"), QString(), tr("Audio (*.mp3 *.flac *.ogg *.opus *.m4a *.wav *.wma);;Todos (*)")));
 });
 connect(folder, &QPushButton::clicked, this, [this]() {
   QString path = QFileDialog::getExistingDirectory(this, tr("Seleccionar carpeta musical"));
   if (path.isEmpty()) return;
   QStringList files;
   QDirIterator it(path, {"*.mp3", "*.flac", "*.ogg", "*.opus", "*.m4a", "*.wav", "*.wma"}, QDir::Files, QDirIterator::Subdirectories);
   while (it.hasNext()) files.append(it.next());
   AddMusicFiles(files);
 });
 connect(remove, &QPushButton::clicked, this, [this]() {
   QStringList selected;
   for (auto *item : libraryTable->selectedItems()) {
     if (item->column() == 0) selected.append(item->data(Qt::UserRole).toString());
   }
   selected.removeDuplicates();
   for (const auto &path : selected) musicFiles.removeAll(path);
   SaveMusicLibrary();
   auto previous = musicFiles;
   musicFiles.clear();
   libraryTable->setRowCount(0);
   AddMusicFiles(previous);
 });
 connect(libraryTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
   auto *item = libraryTable->item(row, 0);
   if (item) PlayLibraryRow(row);
 });
}

void MediaControls::AddMusicFiles(const QStringList &paths)
{
 libraryTable->setSortingEnabled(false);
 for (const QString &path : paths) {
   QFileInfo info(path);
   const QString absolute = info.absoluteFilePath();
   if (!info.isFile() || musicFiles.contains(absolute)) continue;
   musicFiles.append(absolute);
   QString title = info.completeBaseName(), artist, album, year, duration;
   const QByteArray nativePath = QFile::encodeName(absolute);
#ifdef Q_OS_WIN
   const std::wstring widePath = QDir::toNativeSeparators(absolute).toStdWString();
   TagLib::FileRef ref(widePath.c_str());
#else
   TagLib::FileRef ref(nativePath.constData());
#endif
   if (!ref.isNull()) {
     if (auto *tag = ref.tag()) {
       if (!tag->title().isEmpty()) title = QString::fromStdWString(tag->title().toWString());
       artist = QString::fromStdWString(tag->artist().toWString());
       album = QString::fromStdWString(tag->album().toWString());
       if (tag->year() != 0) year = QString::number(tag->year());
     }
     if (auto *audio = ref.audioProperties()) {
       int seconds = audio->lengthInSeconds();
       if (seconds > 0) duration = QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
     }
   }
   int row = libraryTable->rowCount();
   libraryTable->insertRow(row);
   QStringList values{title, artist, album, year, duration};
   for (int col = 0; col < values.size(); ++col) {
     auto *item = new QTableWidgetItem(values.at(col).isEmpty() ? QStringLiteral("—") : values.at(col));
     if (col == 0) { item->setData(Qt::UserRole, absolute); item->setToolTip(absolute); }
     libraryTable->setItem(row, col, item);
   }
 }
 libraryTable->setSortingEnabled(true);
 libraryStatus->setText(tr("%1 canciones · Doble clic: reproducir en VLC").arg(musicFiles.size()));
 UpdateLibraryFilter(librarySearch->text());
 SaveMusicLibrary();
}

void MediaControls::UpdateLibraryFilter(const QString &query)
{
 for (int row = 0; row < libraryTable->rowCount(); ++row) {
   bool match = query.trimmed().isEmpty();
   for (int col = 0; col < libraryTable->columnCount() && !match; ++col) {
     auto *item = libraryTable->item(row, col);
     match = item && item->text().contains(query, Qt::CaseInsensitive);
   }
   libraryTable->setRowHidden(row, !match);
 }
}

void MediaControls::SaveMusicLibrary()
{
 char *config = obs_module_config_path("music-library.json");
 if (!config) return;
 QString path = QString::fromUtf8(config);
 bfree(config);
 QDir().mkpath(QFileInfo(path).absolutePath());
 QJsonArray files;
 for (const auto &entry : musicFiles) files.append(entry);
 QFile out(path);
 if (out.open(QIODevice::WriteOnly)) out.write(QJsonDocument(files).toJson());
}

void MediaControls::LoadMusicLibrary()
{
 char *config = obs_module_config_path("music-library.json");
 if (!config) return;
 QFile in(QString::fromUtf8(config));
 bfree(config);
 if (!in.open(QIODevice::ReadOnly)) return;
 QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
 QStringList paths;
 if (doc.isArray()) for (const auto &value : doc.array()) if (value.isString()) paths.append(value.toString());
 AddMusicFiles(paths);
}


void MediaControls::RefreshVlcSources()
{
 if (!vlcSources) return;
 const QString previous = vlcSources->currentData().toString();
 const QSignalBlocker block(vlcSources);
 vlcSources->clear();
 obs_enum_sources([](void *context, obs_source_t *source) -> bool {
   auto *combo = static_cast<QComboBox *>(context);
   const char *id = obs_source_get_unversioned_id(source);
   if (id && QString::fromUtf8(id) == QStringLiteral("vlc_source")) {
     const QString name = QString::fromUtf8(obs_source_get_name(source));
     combo->addItem(name, name);
   }
   return true;
 }, vlcSources);
 const int idx = vlcSources->findData(previous);
 if (idx >= 0) vlcSources->setCurrentIndex(idx);
}

void MediaControls::PlayLibraryRow(int row)
{
 if (!libraryTable || row < 0 || row >= libraryTable->rowCount()) return;
 SendPlaylistToVlc(row);
}

void MediaControls::SendPlaylistToVlc(int startRow)
{
 if (!vlcSources || vlcSources->currentData().toString().isEmpty()) {
   RefreshVlcSources();
 }
 if (!vlcSources || vlcSources->currentData().toString().isEmpty()) {
   QMessageBox::warning(this, tr("Fuente VLC"), tr("Crea una 'Fuente de vídeo VLC' en OBS y pulsa ↻ para seleccionarla."));
   return;
 }
 const QByteArray name = vlcSources->currentData().toString().toUtf8();
 obs_source_t *source = obs_get_source_by_name(name.constData());
 if (!source) {
   QMessageBox::warning(this, tr("Fuente VLC"), tr("No se encuentra la fuente VLC seleccionada."));
   return;
 }
 obs_data_t *settings = obs_source_get_settings(source);
 obs_data_array_t *playlist = obs_data_array_create();
 // Put the chosen song first, followed by the other songs in table order.
 // VLC handles end-of-track and subsequent items natively.
 for (int offset = 0; offset < libraryTable->rowCount(); ++offset) {
   int row = (startRow + offset) % libraryTable->rowCount();
   auto *item = libraryTable->item(row, 0);
   if (!item) continue;
   const QString path = item->data(Qt::UserRole).toString();
   if (!QFileInfo::exists(path)) continue;
   obs_data_t *entry = obs_data_create();
   const QByteArray utf8 = path.toUtf8();
   obs_data_set_string(entry, "value", utf8.constData());
   obs_data_set_bool(entry, "hidden", false);
   obs_data_array_push_back(playlist, entry);
   obs_data_release(entry);
 }
 obs_data_set_array(settings, "playlist", playlist);
 obs_data_set_bool(settings, "shuffle", false);
 obs_data_array_release(playlist);
 obs_source_update(source, settings);
 obs_source_media_restart(source);
 obs_data_release(settings);
 obs_source_release(source);
 libraryStatus->setText(tr("Reproduciendo en VLC · %1 canciones en cola").arg(libraryTable->rowCount()));
}
