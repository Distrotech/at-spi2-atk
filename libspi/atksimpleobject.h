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

#ifndef __ATK_SIMPLE_OBJECT_H__
#define __ATK_SIMPLE_OBJECT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib-object.h>
#include <atk/atkobject.h>

#define ATK_TYPE_SIMPLE_OBJECT                (atk_simple_object_get_type ())
#define ATK_SIMPLE_OBJECT(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_SIMPLE_OBJECT, AtkSimpleObject))
#define ATK_SIMPLE_OBJECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), ATK_TYPE_SIMPLE_OBJECT, AtkSimpleObjectClass))
#define ATK_IS_SIMPLE_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_SIMPLE_OBJECT))
#define ATK_IS_SIMPLE_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), ATK_TYPE_SIMPLE_OBJECT))
#define ATK_SIMPLE_OBJECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), ATK_TYPE_SIMPLE_OBJECT, AtkSimpleObjectClass))

typedef struct _AtkSimpleObject                   AtkSimpleObject;
typedef struct _AtkSimpleObjectClass              AtkSimpleObjectClass;

struct _AtkSimpleObject
{
  AtkObject     parent;
};

GType atk_simple_object_get_type (void);

struct _AtkSimpleObjectClass
{
  AtkObjectClass parent_class;
};

AtkObject *atk_simple_object_new ();

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ATK_SIMPLE_OBJECT_H__ */

