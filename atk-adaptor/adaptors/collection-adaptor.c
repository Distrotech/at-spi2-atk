/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2007 IBM Corp.
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

/* collection.c: implements the Collection interface */

#include <string.h>

#include <atk/atk.h>
#include <droute/droute.h>

#include "common/bitarray.h"
#include "common/spi-dbus.h"
#include "common/spi-stateset.h"

#include "accessible-register.h"
#include "object.h"

typedef struct _MatchRulePrivate MatchRulePrivate;
struct _MatchRulePrivate
{
  gint *states;
  Accessibility_Collection_MatchType statematchtype;
  AtkAttributeSet *attributes;
  Accessibility_Collection_MatchType attributematchtype;
  gint *roles;
  Accessibility_Collection_MatchType rolematchtype;
  gchar **ifaces;
  Accessibility_Collection_MatchType interfacematchtype;
  gboolean invert;
};

static gboolean
child_interface_p (AtkObject * child, gchar * repo_id)
{
  if (!strcasecmp (repo_id, "action"))
    return atk_is_action (child);
  if (!strcasecmp (repo_id, "component"))
    return atk_is_component (child);
  if (!strcasecmp (repo_id, "editabletext"))
    return atk_is_editable_text (child);
  if (!strcasecmp (repo_id, "text"))
    return atk_is_text (child);
  if (!strcasecmp (repo_id, "hypertext"))
    return atk_is_hypertext (child);
  if (!strcasecmp (repo_id, "image"))
    return atk_is_image (child);
  if (!strcasecmp (repo_id, "selection"))
    return atk_is_selection (child);
  if (!strcasecmp (repo_id, "table"))
    return atk_is_table (child);
  if (!strcasecmp (repo_id, "value"))
    return atk_is_value (child);
  if (!strcasecmp (repo_id, "streamablecontent"))
    return atk_is_streamable_content (child);
  if (!strcasecmp (repo_id, "document"))
    return atk_is_document (child);
  return FALSE;
}

#define child_collection_p(ch) (TRUE)

static gboolean
match_states_all_p (AtkObject * child, gint * set)
{
  AtkStateSet *chs;
  gint i;
  gboolean ret = TRUE;

  if (set == NULL)
    return TRUE;

  chs = atk_object_ref_state_set (child);

  // TODO: use atk-state_set_contains_states; would be more efficient
  for (i = 0; set[i] != BITARRAY_SEQ_TERM; i++)
    {
      if (!atk_state_set_contains_state (chs, set[i]))
        {
          ret = FALSE;
          break;
        }
    }

  g_object_unref (chs);
  return ret;
}

static gboolean
match_states_any_p (AtkObject * child, gint * set)
{
  AtkStateSet *chs;
  gint i;
  gboolean ret = FALSE;

  if (set == NULL)
    return TRUE;

  chs = atk_object_ref_state_set (child);

  for (i = 0; set[i] != BITARRAY_SEQ_TERM; i++)
    {
      if (!atk_state_set_contains_state (chs, set[i]))
        {
          ret = TRUE;
          break;
        }
    }

  g_object_unref (chs);
  return ret;
}

static gboolean
match_states_none_p (AtkObject * child, gint * set)
{
  AtkStateSet *chs;
  gint i;
  gboolean ret = TRUE;

  if (set == NULL)
    return TRUE;

  chs = atk_object_ref_state_set (child);

  for (i = 0; set[i] != BITARRAY_SEQ_TERM; i++)
    {
      if (atk_state_set_contains_state (chs, set[i]))
        {
          ret = FALSE;
          break;
        }
    }

  g_object_unref (chs);
  return ret;
}

// TODO: need to convert at-spi roles/states to atk roles/states */
static gboolean
match_states_lookup (AtkObject * child, MatchRulePrivate * mrp)
{
  switch (mrp->statematchtype)
    {
    case Accessibility_Collection_MATCH_ALL:
      if (match_states_all_p (child, mrp->states))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_ANY:
      if (match_states_any_p (child, mrp->states))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_NONE:
      if (match_states_none_p (child, mrp->states))
        return TRUE;
      break;

    default:
      break;
    }

  return FALSE;
}

// TODO: Map at-spi -> atk roles at mrp creation instead of doing this;
// would be more efficient
#define spi_get_role(obj) spi_accessible_role_from_atk_role(atk_object_get_role(obj))

static gboolean
match_roles_all_p (AtkObject * child, gint * roles)
{
  if (roles == NULL || roles[0] == BITARRAY_SEQ_TERM)
    return TRUE;
  else if (roles[1] != BITARRAY_SEQ_TERM)
    return FALSE;

  return (atk_object_get_role (child) == roles[0]);

}

static gboolean
match_roles_any_p (AtkObject * child, gint * roles)
{
  Accessibility_Role role;
  int i;

  if (roles == NULL || roles[0] == BITARRAY_SEQ_TERM)
    return TRUE;

  role = spi_accessible_role_from_atk_role (atk_object_get_role (child));

  for (i = 0; roles[i] != BITARRAY_SEQ_TERM; i++)
    if (role == roles[i])
      return TRUE;

  return FALSE;
}

static gboolean
match_roles_none_p (AtkObject * child, gint * roles)
{
  AtkRole role;
  int i;

  if (roles == NULL || roles[0] == BITARRAY_SEQ_TERM)
    return TRUE;

  role = atk_object_get_role (child);

  for (i = 0; roles[i] != BITARRAY_SEQ_TERM; i++)
    if (role == roles[i])
      return FALSE;

  return TRUE;
}

static gboolean
match_roles_lookup (AtkObject * child, MatchRulePrivate * mrp)
{
  switch (mrp->rolematchtype)
    {
    case Accessibility_Collection_MATCH_ALL:
      if (match_roles_all_p (child, mrp->roles))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_ANY:
      if (match_roles_any_p (child, mrp->roles))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_NONE:
      if (match_roles_none_p (child, mrp->roles))
        return TRUE;
      break;

    default:
      break;

    }
  return FALSE;
}

static gboolean
match_interfaces_all_p (AtkObject * obj, gchar ** ifaces)
{
  gint i;

  if (ifaces == NULL)
    return TRUE;

  for (i = 0; ifaces[i]; i++)
    if (!child_interface_p (obj, ifaces[i]))
      {
        return FALSE;
      }
  return TRUE;
}

static gboolean
match_interfaces_any_p (AtkObject * obj, gchar ** ifaces)
{
  gint i;

  if (ifaces == NULL)
    return TRUE;


  for (i = 0; ifaces[i]; i++)
    if (child_interface_p (obj, ifaces[i]))
      {
        return TRUE;
      }
  return FALSE;
}

static gboolean
match_interfaces_none_p (AtkObject * obj, gchar ** ifaces)
{
  gint i;

  for (i = 0; ifaces[i]; i++)
    if (child_interface_p (obj, ifaces[i]))
      return FALSE;

  return TRUE;
}

static gboolean
match_interfaces_lookup (AtkObject * child, MatchRulePrivate * mrp)
{
  switch (mrp->interfacematchtype)
    {

    case Accessibility_Collection_MATCH_ALL:
      if (match_interfaces_all_p (child, mrp->ifaces))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_ANY:
      if (match_interfaces_any_p (child, mrp->ifaces))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_NONE:
      if (match_interfaces_none_p (child, mrp->ifaces))
        return TRUE;
      break;

    default:
      break;
    }

  return FALSE;
}

#define split_attributes(attributes) (g_strsplit (attributes, ";", 0))

static gboolean
match_attributes_all_p (AtkObject * child, AtkAttributeSet * attributes)
{
  int i, k;
  AtkAttributeSet *oa;
  gint length, oa_length;
  gboolean flag = FALSE;

  if (attributes == NULL || g_slist_length (attributes) == 0)
    return TRUE;

  oa = atk_object_get_attributes (child);
  length = g_slist_length (attributes);
  oa_length = g_slist_length (oa);

  for (i = 0; i < length; i++)
    {
      AtkAttribute *attr = g_slist_nth_data (attributes, i);
      for (k = 0; k < oa_length; k++)
        {
          AtkAttribute *oa_attr = g_slist_nth_data (attributes, i);
          if (!g_ascii_strcasecmp (oa_attr->name, attr->name) &&
              !g_ascii_strcasecmp (oa_attr->value, attr->value))
            {
              flag = TRUE;
              break;
            }
          else
            flag = FALSE;
        }
      if (!flag)
        {
          atk_attribute_set_free (oa);
          return FALSE;
        }
    }
  atk_attribute_set_free (oa);
  return TRUE;
}

static gboolean
match_attributes_any_p (AtkObject * child, AtkAttributeSet * attributes)
{
  int i, k;

  AtkAttributeSet *oa;
  gint length, oa_length;

  length = g_slist_length (attributes);
  if (length == 0)
    return TRUE;

  oa = atk_object_get_attributes (child);
  oa_length = g_slist_length (oa);

  for (i = 0; i < length; i++)
    {
      AtkAttribute *attr = g_slist_nth_data (attributes, i);
      for (k = 0; k < oa_length; k++)
        {
          AtkAttribute *oa_attr = g_slist_nth_data (attributes, i);
          if (!g_ascii_strcasecmp (oa_attr->name, attr->name) &&
              !g_ascii_strcasecmp (oa_attr->value, attr->value))
            {
              atk_attribute_set_free (oa);
              return TRUE;
            }
        }
    }
  atk_attribute_set_free (oa);
  return FALSE;
}

static gboolean
match_attributes_none_p (AtkObject * child, AtkAttributeSet * attributes)
{
  int i, k;

  AtkAttributeSet *oa;
  gint length, oa_length;

  length = g_slist_length (attributes);
  if (length == 0)
    return TRUE;

  oa = atk_object_get_attributes (child);
  oa_length = g_slist_length (oa);

  for (i = 0; i < length; i++)
    {
      AtkAttribute *attr = g_slist_nth_data (attributes, i);
      for (k = 0; k < oa_length; k++)
        {
          AtkAttribute *oa_attr = g_slist_nth_data (attributes, i);
          if (!g_ascii_strcasecmp (oa_attr->name, attr->name) &&
              !g_ascii_strcasecmp (oa_attr->value, attr->value))
            {
              atk_attribute_set_free (oa);
              return FALSE;
            }
        }
    }
  atk_attribute_set_free (oa);
  return TRUE;
}

static gboolean
match_attributes_lookup (AtkObject * child, MatchRulePrivate * mrp)
{
  switch (mrp->attributematchtype)
    {

    case Accessibility_Collection_MATCH_ALL:
      if (match_attributes_all_p (child, mrp->attributes))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_ANY:
      if (match_attributes_any_p (child, mrp->attributes))
        return TRUE;
      break;

    case Accessibility_Collection_MATCH_NONE:
      if (match_attributes_none_p (child, mrp->attributes))
        return TRUE;
      break;

    default:
      break;
    }
  return FALSE;
}

static gboolean
traverse_p (AtkObject * child, const gboolean traverse)
{
  if (traverse)
    return TRUE;
  else
    return !child_collection_p (child);
}

static int
sort_order_canonical (MatchRulePrivate * mrp, GList * ls,
                      gint kount, gint max,
                      AtkObject * obj, glong index, gboolean flag,
                      AtkObject * pobj, gboolean recurse, gboolean traverse)
{
  gint i = index;
  glong acount = atk_object_get_n_accessible_children (obj);
  gboolean prev = pobj ? TRUE : FALSE;

  for (; i < acount && (max == 0 || kount < max); i++)
    {
      AtkObject *child = atk_object_ref_accessible_child (obj, i);

      g_object_unref (child);
      if (prev && child == pobj)
        {
          return kount;
        }

      if (flag && match_interfaces_lookup (child, mrp)
          && match_states_lookup (child, mrp)
          && match_roles_lookup (child, mrp)
          && match_attributes_lookup (child, mrp))
        {

          ls = g_list_append (ls, child);
          kount++;
        }

      if (!flag)
        flag = TRUE;

      if (recurse && traverse_p (child, traverse))
        kount = sort_order_canonical (mrp, ls, kount,
                                      max, child, 0, TRUE,
                                      pobj, recurse, traverse);
    }
  return kount;
}

static int
sort_order_rev_canonical (MatchRulePrivate * mrp, GList * ls,
                          gint kount, gint max,
                          AtkObject * obj, gboolean flag, AtkObject * pobj)
{
  AtkObject *nextobj;
  AtkObject *parent;
  glong indexinparent;

  /* This breaks us out of the recursion. */
  if (!obj || obj == pobj)
    {
      return kount;
    }

  /* Add to the list if it matches */
  if (flag && match_interfaces_lookup (obj, mrp)
      && match_states_lookup (obj, mrp)
      && match_roles_lookup (obj, mrp) && match_attributes_lookup (obj, mrp))
    {
      ls = g_list_append (ls, obj);
      kount++;
    }

  if (!flag)
    flag = TRUE;

  /* Get the current nodes index in it's parent and the parent object. */
  indexinparent = atk_object_get_index_in_parent (obj);
  parent = atk_object_get_parent (obj);

  if (indexinparent > 0)
    {
      /* there are still some siblings to visit so get the previous sibling
         and get it's last descendant.
         First, get the previous sibling */
      nextobj = atk_object_ref_accessible_child (parent, indexinparent - 1);
      g_object_unref (nextobj);

      /* Now, drill down the right side to the last descendant */
      while (atk_object_get_n_accessible_children (nextobj) > 0)
        {
          nextobj = atk_object_ref_accessible_child (nextobj,
                                                     atk_object_get_n_accessible_children
                                                     (nextobj) - 1);
          g_object_unref (nextobj);
        }
      /* recurse with the last descendant */
      kount = sort_order_rev_canonical (mrp, ls, kount, max,
                                        nextobj, TRUE, pobj);
    }
  else
    {
      /* no more siblings so next node must be the parent */
      kount = sort_order_rev_canonical (mrp, ls, kount, max,
                                        parent, TRUE, pobj);

    }
  return kount;
}

static int
query_exec (MatchRulePrivate * mrp, Accessibility_Collection_SortOrder sortby,
            GList * ls, gint kount, gint max,
            AtkObject * obj, glong index,
            gboolean flag,
            AtkObject * pobj, gboolean recurse, gboolean traverse)
{
  switch (sortby)
    {
    case Accessibility_Collection_SORT_ORDER_CANONICAL:
      kount = sort_order_canonical (mrp, ls, 0, max, obj, index, flag,
                                    pobj, recurse, traverse);
      break;
    case Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL:
      kount = sort_order_canonical (mrp, ls, 0, max, obj, index, flag,
                                    pobj, recurse, traverse);
      break;
    default:
      kount = 0;
      g_warning ("Sort method not implemented yet");
      break;
    }

  return kount;
}

static gboolean
bitarray_to_seq (int *array, int array_count, int **ret)
{
  int out_size = 4;
  int out_count = 0;
  int i, j;
  int *out = (int *) g_malloc (out_size * sizeof (int));

  if (!out)
    return FALSE;
  for (i = 0; i < array_count; i++)
    {
      for (j = 0; j < 32; j++)
        {
          if (array[i] & (1 << j))
            {
              if (out_count == out_size - 2)
                {
                  out_size <<= 1;
                  out = (int *) g_realloc (out, out_size * sizeof (int));
                  if (!out)
                    return FALSE;
                }
              out[out_count++] = i * 32 + j;
            }
        }
    }
  out[out_count] = BITARRAY_SEQ_TERM;
  *ret = out;
  return TRUE;
}


static dbus_bool_t
read_mr (DBusMessageIter * iter, MatchRulePrivate * mrp)
{
  DBusMessageIter mrc, arrayc;
  dbus_uint32_t *array;
  dbus_int32_t matchType;
  int array_count;
  AtkAttribute *attr;
  int i;
  const char *str;
  char *interfaces = NULL;

  dbus_message_iter_recurse (iter, &mrc);
  dbus_message_iter_recurse (&mrc, &arrayc);
  dbus_message_iter_get_fixed_array (&arrayc, &array, &array_count);
  bitarray_to_seq (array, array_count, &mrp->states);
  for (i = 0; mrp->states[i] != BITARRAY_SEQ_TERM; i++)
    {
      mrp->states[i] = spi_atk_state_from_spi_state (mrp->states[i]);
    }
  dbus_message_iter_next (&mrc);
  dbus_message_iter_get_basic (&mrc, &matchType);
  dbus_message_iter_next (&mrc);
  mrp->statematchtype = matchType;;
  /* attributes */
  mrp->attributes = NULL;
  if (dbus_message_iter_get_arg_type (&mrc) == DBUS_TYPE_STRING)
    {
      char *str;
      gchar **attributes;
      gchar **pp;
      dbus_message_iter_get_basic (&mrc, &str);
      attributes = g_strsplit (str, "\n", -1);
      for (pp = attributes; *pp; pp++)
        {
          str = *pp;
          attr = g_new (AtkAttribute, 1);
          if (attr)
            {
              int len = strcspn (str, ":");
              attr->name = g_strndup (str, len);
              if (str[len] == ':')
                {
                  len++;
                  if (str[len] == ' ')
                    len++;
                  attr->value = g_strdup (str + len);
                }
              else
                attr->value = NULL;
              mrp->attributes = g_slist_prepend (mrp->attributes, attr);
            }
        }
      g_strfreev (attributes);
    }
  else
    {
      dbus_message_iter_recurse (&mrc, &arrayc);
      while (dbus_message_iter_get_arg_type (&arrayc) != DBUS_TYPE_INVALID)
        {
          dbus_message_iter_get_basic (&arrayc, &str);
          // TODO: remove this print
          g_print ("Got attribute: %s\n", str);
          attr = g_new (AtkAttribute, 1);
          if (attr)
            {
              int len = strcspn (str, ":");
              attr->name = g_strndup (str, len);
              if (str[len] == ':')
                {
                  len++;
                  if (str[len] == ' ')
                    len++;
                  attr->value = g_strdup (str + len);
                }
              else
                attr->value = NULL;
              mrp->attributes = g_slist_prepend (mrp->attributes, attr);
            }
          dbus_message_iter_next (&arrayc);
        }
    }
  dbus_message_iter_next (&mrc);
  dbus_message_iter_get_basic (&mrc, &matchType);
  mrp->attributematchtype = matchType;;
  dbus_message_iter_next (&mrc);
  /* Get roles and role match */
  dbus_message_iter_recurse (&mrc, &arrayc);
  dbus_message_iter_get_fixed_array (&arrayc, &array, &array_count);
  bitarray_to_seq (array, array_count, &mrp->roles);
  dbus_message_iter_next (&mrc);
  dbus_message_iter_get_basic (&mrc, &matchType);
  mrp->rolematchtype = matchType;;
  dbus_message_iter_next (&mrc);
  /* Get interfaces and interface match */
  dbus_message_iter_get_basic (&mrc, &interfaces);
  dbus_message_iter_next (&mrc);
  mrp->ifaces = g_strsplit (interfaces, ";", 0);
  dbus_message_iter_get_basic (&mrc, &matchType);
  mrp->interfacematchtype = matchType;;
  dbus_message_iter_next (&mrc);
  /* get invert */
  dbus_message_iter_get_basic (&mrc, &mrp->invert);
  dbus_message_iter_next (iter);
  return TRUE;
}

static DBusMessage *
return_and_free_list (DBusMessage * message, GList * ls)
{
  DBusMessage *reply;
  DBusMessageIter iter, iter_array;
  GList *item;

  reply = dbus_message_new_method_return (message);
  if (!reply)
    return NULL;
  dbus_message_iter_init_append (reply, &iter);
  if (!dbus_message_iter_open_container
      (&iter, DBUS_TYPE_ARRAY, "(so)", &iter_array))
    goto oom;
  for (item = ls; item; item = g_list_next (item))
    {
      spi_object_append_reference (&iter_array, ATK_OBJECT (item->data));
    }
  if (!dbus_message_iter_close_container (&iter, &iter_array))
    goto oom;
  g_list_free (ls);
  return reply;
oom:
  // TODO: Handle out of memory
  g_list_free (ls);
  return reply;
}

static void
free_mrp_data (MatchRulePrivate * mrp)
{
  g_free (mrp->states);
  atk_attribute_set_free (mrp->attributes);
  g_free (mrp->roles);
  g_strfreev (mrp->ifaces);
}

static DBusMessage *
GetMatchesFrom (DBusMessage * message,
                AtkObject * current_object,
                MatchRulePrivate * mrp,
                const Accessibility_Collection_SortOrder sortby,
                const dbus_bool_t isrestrict,
                dbus_int32_t count, const dbus_bool_t traverse)
{
  GList *ls = NULL;
  AtkObject *parent;
  glong index = atk_object_get_index_in_parent (current_object);
  gint kount = 0;

  ls = g_list_append (ls, current_object);

  if (!isrestrict)
    {
      parent = atk_object_get_parent (current_object);
      kount = query_exec (mrp, sortby, ls, 0, count, parent, index,
                          FALSE, NULL, TRUE, traverse);
    }
  else
    kount = query_exec (mrp, sortby, ls, 0, count,
                        current_object, 0, FALSE, NULL, TRUE, traverse);

  ls = g_list_remove (ls, ls->data);

  if (sortby == Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL)
    ls = g_list_reverse (ls);

  free_mrp_data (mrp);
  return return_and_free_list (message, ls);
}

/*
  inorder traversal from a given object in the hierarchy
*/

static int
inorder (AtkObject * collection, MatchRulePrivate * mrp,
         GList * ls, gint kount, gint max,
         AtkObject * obj,
         gboolean flag, AtkObject * pobj, dbus_bool_t traverse)
{
  int i = 0;

  /* First, look through the children recursively. */
  kount = sort_order_canonical (mrp, ls, kount, max, obj, 0, TRUE,
                                NULL, TRUE, TRUE);

  /* Next, we look through the right subtree */
  while ((max == 0 || kount < max) && obj != collection)
    {
      AtkObject *parent = atk_object_get_parent (obj);
      i = atk_object_get_index_in_parent (obj);
      kount = sort_order_canonical (mrp, ls, kount, max, parent,
                                    i + 1, TRUE, FALSE, TRUE, TRUE);
      obj = parent;
    }

  if (kount < max)
    {
      kount = sort_order_canonical (mrp, ls, kount, max,
                                    obj, i + 1, TRUE, FALSE, TRUE, TRUE);
    }

  return kount;
}

/*
  GetMatchesInOrder: get matches from a given object in an inorder traversal.
*/

static DBusMessage *
GetMatchesInOrder (DBusMessage * message,
                   AtkObject * current_object,
                   MatchRulePrivate * mrp,
                   const Accessibility_Collection_SortOrder sortby,
                   const dbus_bool_t recurse,
                   dbus_int32_t count, const dbus_bool_t traverse)
{
  GList *ls = NULL;
  AtkObject *obj;
  gint kount = 0;

  ls = g_list_append (ls, current_object);

  obj = ATK_OBJECT(spi_register_path_to_object (spi_global_register, dbus_message_get_path (message)));

  kount = inorder (obj, mrp, ls, 0, count,
                   current_object, TRUE, NULL, traverse);

  ls = g_list_remove (ls, ls->data);

  if (sortby == Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL)
    ls = g_list_reverse (ls);

  free_mrp_data (mrp);
  return return_and_free_list (message, ls);
}

/*
  GetMatchesInBackOrder: get matches from a given object in a
  reverse order traversal.
*/

static DBusMessage *
GetMatchesInBackOrder (DBusMessage * message,
                       AtkObject * current_object,
                       MatchRulePrivate * mrp,
                       const Accessibility_Collection_SortOrder sortby,
                       dbus_int32_t count)
{
  GList *ls = NULL;
  AtkObject *collection;
  gint kount = 0;

  ls = g_list_append (ls, current_object);

  collection = ATK_OBJECT(spi_register_path_to_object (spi_global_register, dbus_message_get_path (message)));

  kount = sort_order_rev_canonical (mrp, ls, 0, count, current_object,
                                    FALSE, collection);

  ls = g_list_remove (ls, ls->data);

  if (sortby == Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL)
    ls = g_list_reverse (ls);

  free_mrp_data (mrp);
  return return_and_free_list (message, ls);
}

static DBusMessage *
GetMatchesTo (DBusMessage * message,
              AtkObject * current_object,
              MatchRulePrivate * mrp,
              const Accessibility_Collection_SortOrder sortby,
              const dbus_bool_t recurse,
              const dbus_bool_t isrestrict,
              dbus_int32_t count, const dbus_bool_t traverse)
{
  GList *ls = NULL;
  AtkObject *obj;
  gint kount = 0;
  ls = g_list_append (ls, current_object);

  if (recurse)
    {
      obj = ATK_OBJECT (atk_object_get_parent (current_object));
      kount = query_exec (mrp, sortby, ls, 0, count,
                          obj, 0, TRUE, current_object, TRUE, traverse);
    }
  else
    {
      obj = ATK_OBJECT (spi_register_path_to_object (spi_global_register, dbus_message_get_path (message)));
      kount = query_exec (mrp, sortby, ls, 0, count,
                          obj, 0, TRUE, current_object, TRUE, traverse);

    }

  ls = g_list_remove (ls, ls->data);

  if (sortby != Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL)
    ls = g_list_reverse (ls);

  free_mrp_data (mrp);
  return return_and_free_list (message, ls);
}

static DBusMessage *
impl_GetMatchesFrom (DBusConnection * bus, DBusMessage * message,
                     void *user_data)
{
  char *current_object_path = NULL;
  AtkObject *current_object;
  DBusMessageIter iter;
  MatchRulePrivate rule;
  dbus_uint32_t sortby;
  dbus_uint32_t tree;
  dbus_int32_t count;
  dbus_bool_t traverse;
  GList *ls = NULL;
  const char *signature;

  signature = dbus_message_get_signature (message);
  if (strcmp (signature, "o(aiisiaiisib)uuib") != 0 &&
      strcmp (signature, "o(aii(as)iaiisib)uuib") != 0)
    {
      return droute_invalid_arguments_error (message);
    }

  dbus_message_iter_init (message, &iter);
  dbus_message_iter_get_basic (&iter, &current_object_path);
  current_object = ATK_OBJECT (spi_register_path_to_object (spi_global_register, current_object_path));
  if (!current_object)
    {
      // TODO: object-not-found error
      return spi_dbus_general_error (message);
    }
  dbus_message_iter_next (&iter);
  if (!read_mr (&iter, &rule))
    {
      return spi_dbus_general_error (message);
    }
  dbus_message_iter_get_basic (&iter, &sortby);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &tree);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &count);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &traverse);
  dbus_message_iter_next (&iter);

  switch (tree)
    {
    case Accessibility_Collection_TREE_RESTRICT_CHILDREN:
      return GetMatchesFrom (message, current_object,
                             &rule, sortby, TRUE, count, traverse);
      break;
    case Accessibility_Collection_TREE_RESTRICT_SIBLING:
      return GetMatchesFrom (message, current_object,
                             &rule, sortby, FALSE, count, traverse);
      break;
    case Accessibility_Collection_TREE_INORDER:
      return GetMatchesInOrder (message, current_object,
                                &rule, sortby, TRUE, count, traverse);
      break;
    default:
      return NULL;
    }
}

static DBusMessage *
impl_GetMatchesTo (DBusConnection * bus, DBusMessage * message,
                   void *user_data)
{
  char *current_object_path = NULL;
  AtkObject *current_object;
  DBusMessageIter iter;
  MatchRulePrivate rule;
  dbus_uint32_t sortby;
  dbus_uint32_t tree;
  dbus_bool_t recurse;
  dbus_int32_t count;
  dbus_bool_t traverse;
  GList *ls = NULL;
  const char *signature;

  signature = dbus_message_get_signature (message);
  if (strcmp (signature, "o(aiisiaiisib)uubib") != 0 &&
      strcmp (signature, "o(aii(as)iaiisib)uubib") != 0)
    {
      return droute_invalid_arguments_error (message);
    }

  dbus_message_iter_init (message, &iter);
  dbus_message_iter_get_basic (&iter, &current_object_path);
  current_object = ATK_OBJECT (spi_register_path_to_object (spi_global_register, current_object_path));
  if (!current_object)
    {
      // TODO: object-not-found error
      return spi_dbus_general_error (message);
    }
  dbus_message_iter_next (&iter);
  if (!read_mr (&iter, &rule))
    {
      return spi_dbus_general_error (message);
    }
  dbus_message_iter_get_basic (&iter, &sortby);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &tree);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &recurse);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &count);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &traverse);
  dbus_message_iter_next (&iter);

  switch (tree)
    {
    case Accessibility_Collection_TREE_RESTRICT_CHILDREN:
      return GetMatchesTo (message, current_object,
                           &rule, sortby, recurse, TRUE, count, traverse);
      break;
    case Accessibility_Collection_TREE_RESTRICT_SIBLING:
      return GetMatchesTo (message, current_object,
                           &rule, sortby, recurse, FALSE, count, traverse);
      break;
    case Accessibility_Collection_TREE_INORDER:
      return GetMatchesInBackOrder (message, current_object,
                                    &rule, sortby, count);
      break;
    default:
      return NULL;
    }
}

static DBusMessage *
impl_GetMatches (DBusConnection * bus, DBusMessage * message, void *user_data)
{
  AtkObject *obj = ATK_OBJECT (spi_register_path_to_object (spi_global_register, dbus_message_get_path (message)));
  DBusMessageIter iter;
  MatchRulePrivate rule;
  dbus_uint32_t sortby;
  dbus_int32_t count;
  dbus_bool_t traverse;
  GList *ls = NULL;
  const char *signature;

  signature = dbus_message_get_signature (message);
  if (strcmp (signature, "(aiisiaiisib)uib") != 0 &&
      strcmp (signature, "(aii(as)iaiisib)uib") != 0)
    {
      return droute_invalid_arguments_error (message);
    }

  dbus_message_iter_init (message, &iter);
  if (!read_mr (&iter, &rule))
    {
      return spi_dbus_general_error (message);
    }
  dbus_message_iter_get_basic (&iter, &sortby);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &count);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &traverse);
  dbus_message_iter_next (&iter);
  ls = g_list_prepend (ls, obj);
  count = query_exec (&rule, sortby, ls, 0, count,
                      obj, 0, TRUE, NULL, TRUE, traverse);
  ls = g_list_remove (ls, ls->data);

  if (sortby == Accessibility_Collection_SORT_ORDER_REVERSE_CANONICAL)
    ls = g_list_reverse (ls);
  free_mrp_data (&rule);
  return return_and_free_list (message, ls);
}

static DRouteMethod methods[] = {
  {impl_GetMatchesFrom, "GetMatchesFrom"},
  {impl_GetMatchesTo, "GetMatchesTo"},
  {impl_GetMatches, "GetMatches"},
  {NULL, NULL}
};

void
spi_initialize_collection (DRoutePath * path)
{
  droute_path_add_interface (path,
                             SPI_DBUS_INTERFACE_COLLECTION, methods, NULL);
};
