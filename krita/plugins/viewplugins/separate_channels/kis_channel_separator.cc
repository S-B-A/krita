/*
 * This file is part of Krita
 *
 * Copyright (c) 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 * ported from Gimp, Copyright (C) 1997 Eiichi Takamori <taka@ma1.seikyou.ne.jp>
 * original pixelize.c for GIMP 0.54 by Tracy Scott
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <limits.h>

#include <stdlib.h>
#include <vector>

#include <klocale.h>
#include <kiconloader.h>
#include <kinstance.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <ktempfile.h>
#include <kdebug.h>
#include <kgenericfactory.h>
#include <knuminput.h>

#include <kis_doc.h>
#include <kis_image.h>
#include "kis_meta_registry.h"
#include <kis_iterators_pixel.h>
#include <kis_layer.h>
#include <kis_transaction.h>
#include <kis_undo_adapter.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kis_progress_subject.h>
#include <kis_progress_display_interface.h>
#include <kis_colorspace.h>
#include <kis_colorspace_factory_registry.h>
#include <kis_view.h>
#include <kis_paint_device.h>
#include <kis_channelinfo.h>

#include "kis_channel_separator.h"

KisChannelSeparator::KisChannelSeparator(KisView * view)
    : m_view(view)
{
}

void KisChannelSeparator::separate(KisProgressDisplayInterface * progress, enumSepAlphaOptions alphaOps, enumSepSource sourceOps, enumSepOutput outputOps, bool downscale, bool toColor)
{
    KisImageSP image = m_view->canvasSubject()->currentImg();
    if (!image) return;

    KisLayerSP layer = image->activeLayer();
    if (!layer) return;

    KisPaintDeviceSP src = image->activeDevice();
    if (!src) return;

    m_cancelRequested = false;
    if ( progress )
        progress -> setSubject(this, true, true);
    emit notifyProgressStage(i18n("Separating image..."), 0);

    KisUndoAdapter * undo = 0;
    KisTransaction * t = 0;
    if ((undo = image->undoAdapter())) {
        t = new KisTransaction(i18n("Separate Image"), src.data());
    }

    KisColorSpace * dstCs = 0;

    Q_UINT32 numberOfChannels = src->nChannels();
    KisColorSpace * srcCs  = src->colorSpace();
    QValueVector<KisChannelInfo *> channels = srcCs->channels();

    // Flatten the image first, if required
    switch(sourceOps) {

        case(ALL_LAYERS):
            image->flatten();
            break;
        default:
            break;
    }

    vKisPaintDeviceSP layers;

    QValueVector<KisChannelInfo *>::const_iterator begin = channels.begin();
    QValueVector<KisChannelInfo *>::const_iterator end = channels.end();


    QRect rect = src->exactBounds();

    int i = 0;
    for (QValueVector<KisChannelInfo *>::const_iterator it = begin; it != end; ++it)
    {

        KisChannelInfo * ch = (*it);

        if (ch->channelType() == KisChannelInfo::ALPHA && alphaOps != CREATE_ALPHA_SEPARATION) {
            // Don't make an separate separation of the alpha channel if the user didn't ask for it.
            continue;
        }

        Q_INT32 channelSize = ch->size();
        Q_INT32 channelPos = ch->pos();
        Q_INT32 destSize = 1;

        KisPaintDeviceSP dev;
        if (toColor) {
            // We don't downscale if we separate to color channels
            dev = new KisPaintDevice(srcCs, "color separations");
        }
        else {
            if (channelSize == 1 || downscale) {
                dev = new KisPaintDevice( KisMetaRegistry::instance()->csRegistry() -> getColorSpace(KisID("GRAYA",""),"" ), "8 bit grayscale sep");
            }
            else {
                dev = new KisPaintDevice( KisMetaRegistry::instance()->csRegistry() -> getColorSpace(KisID("GRAYA16",""),"" ), "16 bit grayscale sep");
                destSize = 2;
            }
        }

        dstCs = dev->colorSpace();

        layers.push_back(dev);

        for (Q_INT32 row = 0; row < rect.height(); ++row) {

            KisHLineIteratorPixel srcIt = src->createHLineIterator(rect.x(), rect.y() + row, rect.width(), false);
            KisHLineIteratorPixel dstIt = dev->createHLineIterator(rect.x(), rect.y() + row, rect.width(), true);

            while( ! srcIt.isDone() )
            {
                if (srcIt.isSelected())
                {
                    if (toColor) {
                        // Copy the complete channel. We know we are using the same color strategy
                        memcpy(dstIt.rawData() + channelPos, srcIt.rawData() + channelPos, channelSize);

                        if (alphaOps == COPY_ALPHA_TO_SEPARATIONS) {
                            //dstCs->setAlpha(dstIt.rawData(), srcIt.rawData()[srcAlphaPos], 1);
                            dstCs->setAlpha(dstIt.rawData(), srcCs->getAlpha(srcIt.rawData()), 1);
                        }
                        else {
                            dstCs->setAlpha(dstIt.rawData(), OPACITY_OPAQUE, 1);
                        }
                    }
                    else {

                        // To grayscale

                        // Decide wether we need downscaling
                        if (channelSize == 1 && destSize == 1) {

                            // Both 8-bit channels

                            memcpy(dstIt.rawData(), srcIt.rawData() + channelPos, 1);

                            if (alphaOps == COPY_ALPHA_TO_SEPARATIONS) {
                                //dstCs->setAlpha(dstIt.rawData(), srcIt.rawData()[srcAlphaPos], 1);
                                dstCs->setAlpha(dstIt.rawData(), srcCs->getAlpha(srcIt.rawData()), 1);
                            }
                            else {
                                dstCs->setAlpha(dstIt.rawData(), OPACITY_OPAQUE, 1);
                            }
                        }
                        else if (channelSize == 2 && destSize == 2) {

                            // Both 16-bit

                            memcpy(dstIt.rawData(), srcIt.rawData() + channelPos, 2);

                            if (alphaOps == COPY_ALPHA_TO_SEPARATIONS) {
                                //memcpy(dstIt.rawData(), srcIt.rawData() + srcAlphaPos, 2);
                                dstCs->setAlpha(dstIt.rawData(), srcCs->getAlpha(srcIt.rawData()), 1);
                            }
                            else {
                                dstCs->setAlpha(dstIt.rawData(), OPACITY_OPAQUE, 1);
                            }
                        }
                        else if (channelSize != 1 && destSize == 1) {
                            // Downscale
                            memset(dstIt.rawData(), srcCs->scaleToU8(srcIt.rawData(), channelPos), 1);

                            // XXX: Do alpha
                            dstCs->setAlpha(dstIt.rawData(), OPACITY_OPAQUE, 1);
                        }
                        else if (channelSize != 2 && destSize == 2) {
                            // Upscale
                            dstIt.rawData()[0] = srcCs->scaleToU8(srcIt.rawData(), channelPos);

                            // XXX: Do alpha
                            dstCs->setAlpha(dstIt.rawData(), OPACITY_OPAQUE, 1);

                        }
                    }
                }
                ++dstIt;
                ++srcIt;
            }
        }
        ++i;

        emit notifyProgress((i * 100) / numberOfChannels);
        if (m_cancelRequested) {
            break;
        }
    }

    vKisPaintDeviceSP_it deviceIt = layers.begin();

    if (!m_cancelRequested) {

        for (QValueVector<KisChannelInfo *>::const_iterator it = begin; it != end; ++it)
        {

            KisChannelInfo * ch = (*it);

            if (ch->channelType() == KisChannelInfo::ALPHA && alphaOps != CREATE_ALPHA_SEPARATION) {
            // Don't make an separate separation of the alpha channel if the user didn't ask for it.
                continue;
            }

            if (outputOps == TO_LAYERS) {
                KisPaintLayerSP l = new KisPaintLayer(image, ch->name(), OPACITY_OPAQUE);
                image->addLayer( dynamic_cast<KisLayer*>(l.data()), image -> rootLayer(), 0);
            }
            else {

                // To images
                // create a document
                // create an image
                // add layer to image
                // show document in new view
            }
        }
        image->notify();
        if (undo) undo -> addCommand(t);
        m_view->canvasSubject()->document()->setModified(true);

        ++deviceIt;

    }

    emit notifyProgressDone();

}

#include "kis_channel_separator.moc"
