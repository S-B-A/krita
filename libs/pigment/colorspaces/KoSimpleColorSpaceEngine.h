/*
 *  Copyright (c) 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _KO_SIMPLE_COLOR_SPACE_ENGINE_H_
#define _KO_SIMPLE_COLOR_SPACE_ENGINE_H_

#include <KoColorSpaceEngine.h>

class KoSimpleColorSpaceEngine : public KoColorSpaceEngine
{
public:
    KoSimpleColorSpaceEngine();
    ~KoSimpleColorSpaceEngine() override;
    KoColorConversionTransformation* createColorTransformation(const KoColorSpace* srcColorSpace,
                                                                       const KoColorSpace* dstColorSpace,
                                                                       KoColorConversionTransformation::Intent renderingIntent,
                                                                       KoColorConversionTransformation::ConversionFlags conversionFlags) const override;
    const KoColorProfile* addProfile(const QString &profile ) override { Q_UNUSED(profile); return 0; }
    const KoColorProfile* addProfile(const QByteArray &data) override { Q_UNUSED(data); return 0; }
    void removeProfile(const QString &profile ) override { Q_UNUSED(profile); }
};

#endif
