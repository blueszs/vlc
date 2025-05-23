/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_INPUT_MANAGER_H_
#define QVLC_INPUT_MANAGER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../qt.hpp"
#include <vlc_cxx_helpers.hpp>
#include <QObject>
#include <QEvent>
#include <QScopedPointer>
#include <QUrl>
#include "player/input_models.hpp"
#include "util/audio_device_model.hpp"
#include "util/varchoicemodel.hpp"
#include "util/vlctick.hpp"

Q_MOC_INCLUDE("util/renderer_manager.hpp")

using vlc_player_locker = vlc_locker<vlc_player_t, vlc_player_Lock, vlc_player_Unlock>;

using SharedVOutThread = vlc_shared_data_ptr_type(vout_thread_t, vout_Hold, vout_Release);
using SharedAOut = vlc_shared_data_ptr_type(audio_output_t, aout_Hold, aout_Release);

class RendererManager;
class QSignalMapper;

class IMEvent : public QEvent
{
public:
    enum event_types {
        FullscreenControlToggle = QEvent::User + IMEventTypeOffset + 1,
        FullscreenControlShow,
        FullscreenControlHide,
        FullscreenControlPlanHide,
    };

    IMEvent( event_types type, input_item_t *p_input = NULL )
        : QEvent( (QEvent::Type)(type) )
    {
        if( (p_item = p_input) != NULL )
            input_item_Hold( p_item );
    }

    virtual ~IMEvent()
    {
        if( p_item )
            input_item_Release( p_item );
    }

    input_item_t *item() const { return p_item; }

private:
    input_item_t *p_item;
};

class PlayerControllerPrivate;
class PlayerController : public QObject
{
    Q_OBJECT

public:
    enum ABLoopState {
        ABLOOP_STATE_NONE= VLC_PLAYER_ABLOOP_NONE,
        ABLOOP_STATE_A = VLC_PLAYER_ABLOOP_A,
        ABLOOP_STATE_B = VLC_PLAYER_ABLOOP_B
    };
    Q_ENUM(ABLoopState)

    enum PlayingState
    {
        PLAYING_STATE_STARTED = VLC_PLAYER_STATE_STARTED,
        PLAYING_STATE_PLAYING = VLC_PLAYER_STATE_PLAYING,
        PLAYING_STATE_PAUSED = VLC_PLAYER_STATE_PAUSED,
        PLAYING_STATE_STOPPING = VLC_PLAYER_STATE_STOPPING,
        PLAYING_STATE_STOPPED = VLC_PLAYER_STATE_STOPPED
    };
    Q_ENUM(PlayingState)

    enum Telekeys{
        TELE_RED = VLC_PLAYER_TELETEXT_KEY_RED,
        TELE_GREEN = VLC_PLAYER_TELETEXT_KEY_GREEN,
        TELE_YELLOW = VLC_PLAYER_TELETEXT_KEY_YELLOW,
        TELE_BLUE = VLC_PLAYER_TELETEXT_KEY_BLUE,
        TELE_INDEX = VLC_PLAYER_TELETEXT_KEY_INDEX
    };
    Q_ENUM(Telekeys)

    //playback
    Q_PROPERTY(PlayingState playingState READ getPlayingState NOTIFY playingStateChanged FINAL)
    Q_PROPERTY(bool isStarted READ isStarted NOTIFY playingStateChanged FINAL)
    Q_PROPERTY(QString name READ getName NOTIFY nameChanged FINAL)
    Q_PROPERTY(float buffering READ getBuffering  NOTIFY bufferingChanged FINAL)
    Q_PROPERTY(float rate READ getRate WRITE setRate NOTIFY rateChanged FINAL)
    Q_PROPERTY(QUrl url READ getUrl NOTIFY inputChanged FINAL)

    Q_PROPERTY(VLCTime time READ getTime WRITE setTime NOTIFY timeChanged FINAL)
    Q_PROPERTY(VLCTime remainingTime READ getRemainingTime NOTIFY remainingTimeChanged FINAL)
    Q_PROPERTY(double position READ getPosition WRITE setPosition NOTIFY positionChanged FINAL)
    Q_PROPERTY(VLCDuration length READ getLength NOTIFY lengthChanged FINAL)

    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged FINAL)
    Q_PROPERTY(bool rewindable READ isRewindable NOTIFY rewindableChanged FINAL)
    Q_PROPERTY(bool pausable READ isPausable NOTIFY pausableChanged FINAL)
    Q_PROPERTY(bool ratechangable READ isRateChangable NOTIFY rateChangableChanged FINAL)

    Q_PROPERTY(bool canRestorePlayback READ canRestorePlayback NOTIFY playbackRestoreQueried FINAL)

    // meta
    Q_PROPERTY(QString title READ getTitle NOTIFY currentMetaChanged FINAL)
    Q_PROPERTY(QString artist READ getArtist NOTIFY currentMetaChanged FINAL)
    Q_PROPERTY(QString album READ getAlbum NOTIFY currentMetaChanged FINAL)
    Q_PROPERTY(QUrl artwork READ getArtwork NOTIFY currentMetaChanged FINAL)

    //tracks
    Q_PROPERTY(TrackListModel* videoTracks READ getVideoTracks CONSTANT FINAL)
    Q_PROPERTY(TrackListModel* audioTracks READ getAudioTracks CONSTANT FINAL)
    Q_PROPERTY(TrackListModel* subtitleTracks READ getSubtitleTracks CONSTANT FINAL)

    Q_PROPERTY(VLCDuration audioDelay READ getAudioDelay WRITE setAudioDelay NOTIFY audioDelayChanged FINAL)
    Q_PROPERTY(VLCDuration subtitleDelay READ getSubtitleDelay WRITE setSubtitleDelay NOTIFY subtitleDelayChanged FINAL)
    Q_PROPERTY(VLCDuration secondarySubtitleDelay READ getSecondarySubtitleDelay WRITE setSecondarySubtitleDelay NOTIFY secondarySubtitleDelayChanged FINAL)
    Q_PROPERTY(int audioDelayMS READ getAudioDelayMS WRITE setAudioDelayMS NOTIFY audioDelayChanged FINAL)
    Q_PROPERTY(int subtitleDelayMS READ getSubtitleDelayMS WRITE setSubtitleDelayMS NOTIFY subtitleDelayChanged FINAL)
    Q_PROPERTY(int secondarySubtitleDelayMS READ getSecondarySubtitleDelayMS WRITE setSecondarySubtitleDelayMS NOTIFY secondarySubtitleDelayChanged FINAL)
    Q_PROPERTY(float subtitleFPS READ getSubtitleFPS WRITE setSubtitleFPS NOTIFY subtitleFPSChanged FINAL)

    //title/chapters/menu
    Q_PROPERTY(TitleListModel* titles READ getTitles CONSTANT FINAL)
    Q_PROPERTY(ChapterListModel* chapters READ getChapters CONSTANT FINAL)

    Q_PROPERTY(bool hasTitles READ hasTitles NOTIFY hasTitlesChanged FINAL)
    Q_PROPERTY(bool hasChapters READ hasChapters NOTIFY hasChaptersChanged FINAL)
    Q_PROPERTY(bool hasMenu READ hasMenu NOTIFY hasMenuChanged FINAL)
    Q_PROPERTY(bool isMenu READ isMenu NOTIFY isMenuChanged FINAL)
    Q_PROPERTY(bool isInteractive READ isInteractive NOTIFY isInteractiveChanged FINAL)

    //programs
    Q_PROPERTY(ProgramListModel* programs READ getPrograms CONSTANT FINAL)

    Q_PROPERTY(bool hasPrograms READ hasPrograms NOTIFY hasProgramsChanged FINAL)
    Q_PROPERTY(bool isEncrypted READ isEncrypted NOTIFY isEncryptedChanged FINAL)

    //teletext
    Q_PROPERTY(bool teletextEnabled READ isTeletextEnabled WRITE enableTeletext NOTIFY teletextEnabledChanged FINAL)
    Q_PROPERTY(bool isTeletextAvailable READ isTeletextAvailable  NOTIFY teletextAvailableChanged FINAL)
    Q_PROPERTY(int teletextPage READ getTeletextPage WRITE setTeletextPage NOTIFY teletextPageChanged FINAL)
    Q_PROPERTY(bool teletextTransparency READ getTeletextTransparency WRITE setTeletextTransparency NOTIFY teletextTransparencyChanged FINAL)

    //vout properties
    Q_PROPERTY(bool hasVideoOutput READ hasVideoOutput NOTIFY hasVideoOutputChanged FINAL)
    Q_PROPERTY(VLCVarChoiceModel* videoStereoMode READ getVideoStereoMode CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* zoom READ getZoom CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* aspectRatio READ getAspectRatio CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* crop READ getCrop CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* fit READ getFit CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* deinterlace READ getDeinterlace CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* deinterlaceMode READ getDeinterlaceMode CONSTANT FINAL)
    Q_PROPERTY(bool fullscreen READ isFullscreen WRITE setFullscreen NOTIFY fullscreenChanged FINAL)
    Q_PROPERTY(bool wallpaperMode READ getWallpaperMode WRITE setWallpaperMode NOTIFY wallpaperModeChanged FINAL)
    Q_PROPERTY(bool autoscale READ getAutoscale WRITE setAutoscale NOTIFY autoscaleChanged FINAL)

    //aout properties
    Q_PROPERTY(float volume READ getVolume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY soundMuteChanged FINAL)
    Q_PROPERTY(AudioDeviceModel* audioDevices READ getAudioDevices CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* audioStereoMode READ getAudioStereoMode CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* audioMixMode READ getAudioMixMode CONSTANT FINAL)
    Q_PROPERTY(VLCVarChoiceModel* audioVisualization READ getAudioVisualizations CONSTANT FINAL)
    Q_PROPERTY(bool hasAudioVisualization READ hasAudioVisualization NOTIFY hasAudioVisualizationChanged FINAL)

    //misc
    Q_PROPERTY(ABLoopState ABloopState READ getABloopState WRITE setABloopState NOTIFY ABLoopStateChanged FINAL)
    Q_PROPERTY(VLCTime ABLoopA READ getABLoopA NOTIFY ABLoopAChanged FINAL)
    Q_PROPERTY(VLCTime ABLoopB READ getABLoopB NOTIFY ABLoopBChanged FINAL)
    Q_PROPERTY(bool recording READ isRecording WRITE setRecording NOTIFY recordingChanged FINAL)
    Q_PROPERTY(RendererManager* rendererManager READ getRendererManager CONSTANT FINAL)

    // High resolution time fed by SMPTE Timer
    Q_PROPERTY(QString highResolutionTime READ highResolutionTime FINAL)

    /* exposed actions */
public slots:
    void reverse();
    void slower();
    void faster();
    void littlefaster();
    void littleslower();
    void normalRate();

    void jumpFwd();
    void jumpBwd();
    void jumpToTime( VLCTime i_time );
    void jumpToPos( double );
    void frameNext();

    //title/chapters/menu
    void sectionNext();
    void sectionPrev();
    void sectionMenu();

    void chapterNext();
    void chapterPrev();
    void titleNext();
    void titlePrev();

    //programs
    void changeProgram( int );

    //menu navigation
    Q_INVOKABLE void navigateUp( );
    Q_INVOKABLE void navigateDown( );
    Q_INVOKABLE void navigateLeft( );
    Q_INVOKABLE void navigateRight( );
    Q_INVOKABLE void navigateActivate( );

    //vout properties
    void toggleFullscreen();

    //aout properties
    void setVolumeUp( int steps = 1 );
    void setVolumeDown( int steps = 1 );
    void toggleMuted();

    //misc
    void toggleABloopState();
    void snapshot();
    void toggleRecord();
    void toggleVisualization();

    // SMPTE Timer
    void requestAddSMPTETimer();
    void requestRemoveSMPTETimer();

public:
    PlayerController( qt_intf_t * );
    ~PlayerController();

public:
    using VOutThreadList = QVector<SharedVOutThread>;


public:
    vlc_player_t * getPlayer() const;

    input_item_t *getInput() const;

    SharedVOutThread getVout();
    VOutThreadList getVouts() const;
    SharedAOut getAout();
    int AddAssociatedMedia(enum es_format_category_e cat, const QString& uri, bool select, bool notify, bool check_ext);

    void requestArtUpdate( input_item_t *p_item );
    void setArt( input_item_t *p_item, QString fileUrl );
    static const QString decodeArtURL( input_item_t *p_item );
    void updatePosition();
    void updateTime(vlc_tick_t system_now, bool forceTimer);

    //getter/setters binded to a Q_PROPERTY
public slots:
    //playback
    PlayingState getPlayingState() const;
    bool isStarted() const;
    bool hasInput() const;
    QString getName() const;
    float getBuffering() const;
    float getRate() const;
    void setRate( float );
    VLCTime getTime() const;
    void setTime(VLCTime);
    VLCDuration getRemainingTime() const;
    double getPosition() const;
    void setPosition(double);
    VLCDuration getLength() const;
    bool isSeekable() const;
    bool isRewindable() const;
    bool isPausable() const;
    bool isRateChangable() const;
    void updatePositionFromTimer();
    void updateTimeFromTimer();
    bool canRestorePlayback() const;
    void restorePlaybackPos();
    void openVLsub();
    void acknowledgeRestoreCallback();
    QUrl getUrl() const;

    //tracks
    TrackListModel* getVideoTracks();
    TrackListModel* getAudioTracks();
    TrackListModel* getSubtitleTracks();

    VLCDuration getAudioDelay() const;
    void setAudioDelay( VLCDuration );
    Q_INVOKABLE void addAudioDelay( VLCDuration );
    VLCDuration getSubtitleDelay() const;
    VLCDuration getSecondarySubtitleDelay() const;
    void setSubtitleDelay( VLCDuration );
    Q_INVOKABLE void addSubtitleDelay( VLCDuration );
    void setSecondarySubtitleDelay( VLCDuration );
    Q_INVOKABLE void addSecondarySubtitleDelay( VLCDuration );
    int getAudioDelayMS() const;
    void setAudioDelayMS( int );
    int getSubtitleDelayMS() const;
    void setSubtitleDelayMS( int );
    int getSecondarySubtitleDelayMS() const;
    void setSecondarySubtitleDelayMS( int );
    float getSubtitleFPS( ) const;
    void setSubtitleFPS( float );

    //title/chapters/menu
    TitleListModel* getTitles();
    ChapterListModel* getChapters();
    bool hasTitles() const;
    bool hasChapters() const;
    bool hasMenu()  const;
    bool isMenu()  const;
    bool isInteractive()  const;

    //programs
    ProgramListModel* getPrograms();
    bool hasPrograms() const;
    bool isEncrypted() const;

    //teletext
    bool isTeletextEnabled() const;
    void enableTeletext(bool enable);
    bool isTeletextAvailable() const;
    int getTeletextPage() const;
    void setTeletextPage(int page);
    bool getTeletextTransparency() const;
    void setTeletextTransparency( bool transparent );

    //vout properties
    bool hasVideoOutput() const;
    VLCVarChoiceModel* getVideoStereoMode();
    VLCVarChoiceModel* getZoom();
    VLCVarChoiceModel* getAspectRatio();
    VLCVarChoiceModel* getCrop();
    VLCVarChoiceModel* getFit();
    VLCVarChoiceModel* getDeinterlace();
    VLCVarChoiceModel* getDeinterlaceMode();
    bool isFullscreen() const;
    void setFullscreen( bool );
    bool getWallpaperMode() const;
    void setWallpaperMode( bool );
    bool getAutoscale() const;
    void setAutoscale( bool );

    //aout properties
    float getVolume() const;
    void setVolume( float volume );
    bool isMuted() const;
    void setMuted( bool muted );
    AudioDeviceModel* getAudioDevices();
    VLCVarChoiceModel* getAudioStereoMode();
    VLCVarChoiceModel* getAudioMixMode();
    VLCVarChoiceModel* getAudioVisualizations();
    bool hasAudioVisualization() const;


    //misc
    bool isRecording() const;
    void setRecording(bool record);
    void setABloopState(ABLoopState);
    ABLoopState getABloopState() const;
    VLCTime getABLoopA() const;
    VLCTime getABLoopB() const;

    // High resolution time fed by SMPTE timer
    QString highResolutionTime() const;

    // associates subtitle file to currently playing media
    // returns true on success
    bool associateSubtitleFile(const QString &uri);

    // meta
    QString getTitle() const;
    QString getArtist() const;
    QString getAlbum() const;
    QUrl getArtwork() const;

    //Renderer
    RendererManager* getRendererManager();

signals:
    //playback
    void playingStateChanged( PlayingState state );
    void inputChanged( bool hasInput );
    void nameChanged( const QString& );
    void bufferingChanged( float );
    void rateChanged( float );

    void timeChanged( VLCTime );
    void remainingTimeChanged( VLCTime );
    void positionChanged( double );
    void lengthChanged( VLCDuration );
    void positionUpdated( double , VLCTime, VLCDuration );
    void seekRequested( double pos ); //not exposed through Q_PROPERTY

    void seekableChanged( bool );
    void rewindableChanged( bool );
    void pausableChanged( bool );
    void recordableChanged( bool );
    void rateChangableChanged( bool );

    void playbackRestoreQueried();

    //tracks
    void audioDelayChanged(VLCDuration);
    void subtitleDelayChanged(VLCDuration);
    void secondarySubtitleDelayChanged(VLCDuration);
    void subtitleFPSChanged(float);

    //title/chapters/menu
    void hasTitlesChanged( bool );
    void hasChaptersChanged( bool );
    void hasMenuChanged( bool );
    void isMenuChanged( bool );
    void isInteractiveChanged( bool );

    //program
    void hasProgramsChanged( bool );
    void isEncryptedChanged( bool );

    //teletext
    void teletextEnabledChanged(bool);
    void teletextAvailableChanged(bool);
    void teletextPageChanged(int);
    void teletextTransparencyChanged(bool);

    //vout properties
    void hasVideoOutputChanged( bool );
    void fullscreenChanged( bool );
    void wallpaperModeChanged( bool );
    void autoscaleChanged( bool );
    void voutListChanged( vout_thread_t **pp_vout, int i_vout );

    //aout properties
    void volumeChanged( float );
    void soundMuteChanged( bool );
    void hasAudioVisualizationChanged( bool );

    //misc
    void recordingChanged( bool );
    void ABLoopStateChanged(ABLoopState);
    void ABLoopAChanged(VLCTime);
    void ABLoopBChanged(VLCTime);

    // High resolution time fed by SMPTE timer
    void highResolutionTimeChanged(const QString&);

    // Other signals

    // You can resume playback
    void resumePlayback( VLCTime );
    // Statistics are updated
    void statisticsUpdated( const input_stats_t& stats );
    void infoChanged( input_item_t* );
    void currentMetaChanged( input_item_t* );
    void metaChanged( input_item_t *);
    void artChanged( QString ); /* current item art ( same as item == NULL ) */
    void artChanged( input_item_t * );

    void bookmarksChanged();
    // Program Event changes
    void epgChanged();

private:
    Q_DECLARE_PRIVATE(PlayerController)
    QScopedPointer<PlayerControllerPrivate> d_ptr;
};

#endif
