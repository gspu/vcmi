/*
 * TextControls.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "VideoWidget.h"
#include "TextControls.h"

#include "../CGameInfo.h"
#include "../gui/CGuiHandler.h"
#include "../media/ISoundPlayer.h"
#include "../media/IVideoPlayer.h"
#include "../render/Canvas.h"

#include "../../lib/filesystem/Filesystem.h"

VideoWidgetBase::VideoWidgetBase(const Point & position, const VideoPath & video, bool playAudio)
	: VideoWidgetBase(position, video, playAudio, 1.0)
{
}

VideoWidgetBase::VideoWidgetBase(const Point & position, const VideoPath & video, bool playAudio, float scaleFactor)
	: playAudio(playAudio), scaleFactor(scaleFactor)
{
	addUsedEvents(TIME);
	pos += position;
	playVideo(video);
}

VideoWidgetBase::~VideoWidgetBase() = default;

void VideoWidgetBase::playVideo(const VideoPath & fileToPlay)
{
	OBJECT_CONSTRUCTION;

	JsonPath subTitlePath = fileToPlay.toType<EResType::JSON>();
	JsonPath subTitlePathVideoDir = subTitlePath.addPrefix("VIDEO/");
	if(CResourceHandler::get()->existsResource(subTitlePath))
		subTitleData = JsonNode(subTitlePath);
	else if(CResourceHandler::get()->existsResource(subTitlePathVideoDir))
		subTitleData = JsonNode(subTitlePathVideoDir);

	videoInstance = CCS->videoh->open(fileToPlay, scaleFactor);
	if (videoInstance)
	{
		pos.w = videoInstance->size().x;
		pos.h = videoInstance->size().y;
		subTitle = std::make_unique<CMultiLineLabel>(Rect(0, (pos.h / 5) * 4, pos.w, pos.h / 5), EFonts::FONT_HIGH_SCORE, ETextAlignment::CENTER, Colors::WHITE);
	}

	if (playAudio)
	{
		loadAudio(fileToPlay);
		if (isActive())
			startAudio();
	}
}

void VideoWidgetBase::show(Canvas & to)
{
	if(videoInstance)
		videoInstance->show(pos.topLeft(), to);
	if(subTitle)
		subTitle->showAll(to);
}

void VideoWidgetBase::loadAudio(const VideoPath & fileToPlay)
{
	if (!playAudio)
		return;

	audioData = CCS->videoh->getAudio(fileToPlay);
}

void VideoWidgetBase::startAudio()
{
	if(audioData.first == nullptr)
		return;

	audioHandle = CCS->soundh->playSound(audioData);

	if(audioHandle != -1)
	{
		CCS->soundh->setCallback(
			audioHandle,
			[this]()
			{
				this->audioHandle = -1;
			}
		);
	}
}

void VideoWidgetBase::stopAudio()
{
	if(audioHandle != -1)
	{
		CCS->soundh->resetCallback(audioHandle);
		CCS->soundh->stopSound(audioHandle);
		audioHandle = -1;
	}
}

std::string VideoWidgetBase::getSubTitleLine(double timestamp)
{
	if(subTitleData.isNull())
		return {};

	for(auto & segment : subTitleData.Vector())
		if(timestamp > segment["timeStart"].Float() && timestamp < segment["timeEnd"].Float())
			return segment["text"].String();
	
	return {};
}

void VideoWidgetBase::activate()
{
	CIntObject::activate();
	startAudio();
}

void VideoWidgetBase::deactivate()
{
	CIntObject::deactivate();
	stopAudio();
}

void VideoWidgetBase::showAll(Canvas & to)
{
	if(videoInstance)
		videoInstance->show(pos.topLeft(), to);
	if(subTitle)
		subTitle->showAll(to);
}

void VideoWidgetBase::tick(uint32_t msPassed)
{
	if(videoInstance)
	{
		videoInstance->tick(msPassed);

		if(videoInstance->videoEnded())
		{
			videoInstance.reset();
			stopAudio();
			onPlaybackFinished();
		}
	}
	if(subTitle && videoInstance)
		subTitle->setText(getSubTitleLine(videoInstance->timeStamp()));
}

VideoWidget::VideoWidget(const Point & position, const VideoPath & prologue, const VideoPath & looped, bool playAudio)
	: VideoWidgetBase(position, prologue, playAudio)
	, loopedVideo(looped)
{
}

VideoWidget::VideoWidget(const Point & position, const VideoPath & looped, bool playAudio)
	: VideoWidgetBase(position, looped, playAudio)
	, loopedVideo(looped)
{
}

void VideoWidget::onPlaybackFinished()
{
	playVideo(loopedVideo);
}

VideoWidgetOnce::VideoWidgetOnce(const Point & position, const VideoPath & video, bool playAudio, const std::function<void()> & callback)
	: VideoWidgetBase(position, video, playAudio)
	, callback(callback)
{
}

VideoWidgetOnce::VideoWidgetOnce(const Point & position, const VideoPath & video, bool playAudio, float scaleFactor, const std::function<void()> & callback)
	: VideoWidgetBase(position, video, playAudio, scaleFactor)
	, callback(callback)
{
}

void VideoWidgetOnce::onPlaybackFinished()
{
	callback();
}
