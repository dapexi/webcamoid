/* Webcamoid, webcam capture application.
 * Copyright (C) 2015  Gonzalo Exequiel Pedone
 *
 * Webcamoid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Webcamoid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Webcamoid. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

#include <QMutex>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <akutils.h>

#include "videodisplay.h"

class VideoDisplayPrivate
{
    public:
        QImage m_frame;
        QMutex m_mutex;
        bool m_fillDisplay;

        VideoDisplayPrivate():
            m_fillDisplay(false)
        {
        }
};

VideoDisplay::VideoDisplay(QQuickItem *parent):
    QQuickItem(parent)
{
    this->d = new VideoDisplayPrivate;
    this->setFlag(ItemHasContents, true);
}

VideoDisplay::~VideoDisplay()
{
    delete this->d;
}

bool VideoDisplay::fillDisplay() const
{
    return this->d->m_fillDisplay;
}

QSGNode *VideoDisplay::updatePaintNode(QSGNode *oldNode,
                                       QQuickItem::UpdatePaintNodeData *updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData)

    this->d->m_mutex.lock();

    if (this->d->m_frame.isNull()) {
        this->d->m_mutex.unlock();

        return nullptr;
    }

    auto frame = this->d->m_frame.format() == QImage::Format_ARGB32?
                     this->d->m_frame.copy():
                     this->d->m_frame.convertToFormat(QImage::Format_ARGB32);
    this->d->m_mutex.unlock();

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    if (this->window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
#endif
        frame = frame.scaled(this->boundingRect().size().toSize(),
                             this->d->m_fillDisplay?
                                 Qt::IgnoreAspectRatio:
                                 Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    }
#endif

    auto videoFrame = this->window()->createTextureFromImage(frame);

    if (!videoFrame || videoFrame->textureSize().isEmpty()) {
        if (oldNode)
            delete oldNode;

        if (videoFrame)
            delete videoFrame;

        return nullptr;
    }

    QSGSimpleTextureNode *node = nullptr;

    if (oldNode)
        node = static_cast<QSGSimpleTextureNode *>(oldNode);
    else
        node = new QSGSimpleTextureNode();

    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);

    if (this->d->m_fillDisplay)
        node->setRect(this->boundingRect());
    else {
        QSizeF size(videoFrame->textureSize());
        size.scale(this->boundingRect().size(), Qt::KeepAspectRatio);
        QRectF rect(QPointF(), size);
        rect.moveCenter(this->boundingRect().center());

        node->setRect(rect);
    }

    node->setTexture(videoFrame);

    return node;
}

void VideoDisplay::iStream(const AkPacket &packet)
{
    this->d->m_mutex.lock();
    this->d->m_frame = AkUtils::packetToImage(packet);
    this->d->m_mutex.unlock();

    QMetaObject::invokeMethod(this, "update");
}

void VideoDisplay::setFillDisplay(bool fillDisplay)
{
    if (this->d->m_fillDisplay == fillDisplay)
        return;

    this->d->m_fillDisplay = fillDisplay;
    emit this->fillDisplayChanged();
}

void VideoDisplay::resetFillDisplay()
{
    this->setFillDisplay(false);
}

#include "moc_videodisplay.cpp"
