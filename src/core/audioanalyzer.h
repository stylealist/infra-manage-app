/***************************************************************************
 audioanalyzer.h - AudioAnalyzer

 ---------------------
 begin                : 12.04.2026
 copyright            : (C) 2026 by Mathieu Pellerin
 email                : mathieu (at) opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AUDIOANALYZER_H
#define AUDIOANALYZER_H

#include <QAudioDecoder>
#include <QObject>
#include <QThread>
#include <QUrl>

class AudioPeaksGatherer : public QThread
{
    Q_OBJECT

  public:
    AudioPeaksGatherer( const QUrl &url );

    void run() override;

    void stop();

    QList<float> rawPeaks() const { return mRawPeaks; }

  signals:
    void collectedRawPeaks();

  private slots:
    void processBuffer();
    void finalize();

  private:
    QUrl mUrl;
    QAudioDecoder *mDecoder = nullptr;
    QVector<float> mRawPeaks;
};

/**
 * \ingroup core
 */
class AudioAnalyzer : public QObject
{
    Q_OBJECT

  public:
    explicit AudioAnalyzer( QObject *parent = nullptr );

    Q_INVOKABLE void analyze( const QUrl &source );

  signals:
    void ready( const QList<qreal> &bars );

  private slots:
    void finalize();
    void gathererThreadFinished();

  private:
    AudioPeaksGatherer *mGatherer = nullptr;
};

#endif // AUDIORECORDER_H
