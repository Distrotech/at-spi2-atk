/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SPI_STATE_SET_H_
#define SPI_STATE_SET_H_

#include <libspi/base.h>
#include <atk/atkstateset.h>

G_BEGIN_DECLS

#define SPI_STATE_SET_TYPE            (spi_state_set_get_type ())
#define SPI_STATE_SET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPI_STATE_SET_TYPE, SpiStateSet))
#define SPI_STATE_SET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPI_STATE_SET_TYPE, SpiStateSetClass))
#define SPI_IS_STATE_SET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPI_STATE_SET_TYPE))
#define SPI_IS_STATE_SET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPI_STATE_SET_TYPE))

typedef struct _SpiStateSet SpiStateSet;
typedef struct _SpiStateSetClass SpiStateSetClass;

struct _SpiStateSet {
  SpiBase parent;
};

struct _SpiStateSetClass {
  SpiBaseClass parent_class;
  POA_Accessibility_StateSet__epv epv;
};

GType        spi_state_set_get_type (void);
SpiStateSet *spi_state_set_new      (AtkStateSet *set);

G_END_DECLS

#endif /* SPI_STATE_SET_H_ */
