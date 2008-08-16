/*
  pygame physics - Pygame physics module

  Copyright (C) 2008 Zhang Fan

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define PHYSICS_WORLD_INTERNAL
#include "pgDeclare.h"
#include "pgphysics.h"
#include "pgVector2.h"
#include "pgAABBBox.h"
#include "pgBodyObject.h"
#include "pgJointObject.h"
#include "pgWorldObject.h"
#include "pgShapeObject.h"
#include "pgCollision.h"
#include "pgHelpFunctions.h"

#define MAX_ITERATION 5

static void _FreeBodySimulation(PyWorldObject* world,double stepTime);
static void _BodyCollisionDetection(PyWorldObject* world, double step);
static void _JointSolve(PyWorldObject* world,double stepTime);
static void _BodyPositionUpdate(PyWorldObject* world,double stepTime);
static void _BodyPositionCorrection(PyWorldObject* world,double stepTime);
static void _Update(PyWorldObject* world,double stepTime);

static void _WorldInit(PyWorldObject* world);
static PyWorldObject* _WorldNewInternal(PyTypeObject *type);
static PyObject* _WorldNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void _WorldDestroy(PyWorldObject* world);

static PyObject* _World_update (PyWorldObject* world, PyObject* args);
static PyObject* _World_addBody(PyWorldObject* world,PyObject* args);
static PyObject* _World_removeBody(PyWorldObject* world,PyObject* args);
static PyObject* _World_addJoint(PyWorldObject* world,PyObject* args);
static PyObject* _World_removeJoint(PyWorldObject* world,PyObject* args);
static PyObject* _World_getGravity(PyWorldObject* world,void* closure);
static int _World_setGravity(PyWorldObject* world,PyObject* value,
    void* closure);
static PyObject* _World_getDamping(PyWorldObject* world,void* closure);
static int _World_setDamping(PyWorldObject* world,PyObject* value,
    void* closure);
static PyObject* _World_getBodyList(PyWorldObject* world,void* closure);
static PyObject* _World_getJointList(PyWorldObject* world,void* closure);

/* C API */
static PyObject* PyWorld_New (void);
static int PyWorld_AddBody(PyObject* world, PyObject* body);
static int PyWorld_RemoveBody(PyObject* world, PyObject* body);
static int PyWorld_AddJoint(PyObject* world, PyObject* joint);
static int PyWorld_RemoveJoint(PyObject* world, PyObject* joint);
static int PyWorld_Update(PyObject* world, double dt);

/**
 * Getters and setters for the World
 */
static PyGetSetDef _World_getseters[] = {
    { "damping", (getter)_World_getDamping, (setter)_World_setDamping,
      "The damping within the world. Currently not implemented.", NULL },
    { "gravity",(getter)_World_getGravity, (setter)_World_setGravity,
      "The gravity vector of the world as a (x, y) tuple,", NULL, },
    { "bodies",(getter)_World_getBodyList,NULL,
      "Read-only list of bodies currently attached to the world", NULL },
    { "joints",(getter)_World_getJointList,NULL,
      "Read-only list of joints currently attached to the world",NULL },
    { NULL, NULL, NULL, NULL, NULL }
};

/**
 * Methods for the world.
 */
static PyMethodDef _World_methods[] =
{
    { "update", (PyCFunction) _World_update, METH_VARARGS,
      "W.update (steptime=0.1) -> None\n\n"
      "Performs a simulation for the whole world.\n\n"
      "Simulates the movements, collisions and anything else for all bodies\n"
      "and joints within the world. The steptime represents the resolution\n"
      "for the simulation. Thus a higher steptime will result in a faster,\n"
      "but also more inexact simulation, while a low steptime will cause\n"
      "more precise simulation results."
    },
    { "add_body",(PyCFunction) _World_addBody, METH_VARARGS,
      "W.add_body (body) -> None\n\n"
      "Adds a body to the world. If the body was already added, this raises\n"
      "a ValueError."},
     {"add_joint",(PyCFunction) _World_addJoint, METH_VARARGS,
      "W.add_joint (joint) -> None\n\n"
      "Adds a joint to the world. If the joint was already added, this raises\n"
      "a ValueError."},
    { "remove_body",(PyCFunction) _World_removeBody, METH_VARARGS,
      "W.remove_body (body) -> None\n\n"
      "Removes a body from the world. If the body was not added, this raises\n"
      "a ValueError."},
     {"remove_joint",(PyCFunction) _World_removeJoint, METH_VARARGS,
      "W.remove_joint (joint) -> None\n\n"
      "Remove a joint from the world. If the joint was not added, this raises\n"
      "a ValueError."},
    { NULL, NULL, 0, NULL } /* The NULL sentinel is important! */
};

PyTypeObject PyWorld_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "physics.World",            /* tp_name */
    sizeof(PyWorldObject),      /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor) _WorldDestroy, /* tp_dealloc */
    0,                          /* tp_print */
    0,                          /* tp_getattr */
    0,                          /* tp_setattr */
    0,                          /* tp_compare */
    0,                          /* tp_repr */
    0,                          /* tp_as_number */
    0,                          /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash */
    0,                          /* tp_call */
    0,                          /* tp_str */
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
    0,                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "World () -> World\n\n"
    "Creates a new World object.",
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    _World_methods,             /* tp_methods */
    0,                          /* tp_members */
    _World_getseters,           /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    _WorldNew,                  /* tp_new */
    0,                          /* tp_free */
    0,                          /* tp_is_gc */
    0,                          /* tp_bases */
    0,                          /* tp_mro */
    0,                          /* tp_cache */
    0,                          /* tp_subclasses */
    0,                          /* tp_weaklist */
    0                           /* tp_del */
};

/**
 * Updates the velocity of the bodies attached to the world.
 *
 * @param world The PyWorldObject to update the bodies for.
 * @param stepTime The time passed since the last update.
 */ 
static void _FreeBodySimulation(PyWorldObject* world,double stepTime)
{
    Py_ssize_t i;
    PyBodyObject* body;
    Py_ssize_t size = PyList_Size((PyObject*)(world->bodyList));
    for (i = 0; i < size; ++i)
    {
        body = (PyBodyObject*)(PyList_GetItem(world->bodyList, i));		
        PyBodyObject_FreeUpdateVel(body, world->vecGravity, stepTime);
    }
}

/**
 * Checks the bodies and joints attached to the world for collisions and
 * updates them accordingly.
 *
 * @param world The PyWorldObject to check the bodies and joints for.
 * @param step The time passed since the last update.
 */
static void _BodyCollisionDetection(PyWorldObject* world, double step)
{
    Py_ssize_t i, j, body_cnt, contact_cnt;
    PyBodyObject *refBody, *incBody;
    PyJointObject* contact;
    PyShapeObject *refShape, *incShape;

    body_cnt = PyList_Size(world->bodyList);
    //clear contactList first
    contact_cnt = PyList_Size(world->contactList);
    if(PyList_SetSlice(world->contactList, 0, contact_cnt, NULL) < 0)
        return;
    assert(PyList_Size(world->contactList)==0);
	
    //for all pair of objects, do collision test	
    //update AABB
    for(i = 0; i < body_cnt; ++i)
    {
        refBody = (PyBodyObject*)(PyList_GetItem(world->bodyList, i));
        PyShapeObject_UpdateAABB (refBody);
    }
    
    //collision test
    for(i = 0; i < body_cnt-1; ++i)
    {
        refBody = (PyBodyObject*)(PyList_GetItem(world->bodyList, i));
        for(j = i+1; j < body_cnt; ++j)
        {
            incBody = (PyBodyObject*)(PyList_GetItem(world->bodyList, j));
            if(refBody->bStatic && incBody->bStatic)
                continue;
            refShape = (PyShapeObject*)refBody->shape;
            incShape = (PyShapeObject*)incBody->shape;
            if(AABB_IsOverlap(&(refShape->box), &(incShape->box), 1e-8))
            {
                PyShapeObject_Collision (refBody, incBody, world->contactList);
            }
        }
    }

    contact_cnt = PyList_Size(world->contactList);
/*     if (contact_cnt) */
/*     { */
/*         printf("contact_cnt:%d\n",contact_cnt); */
/*     } */
    for(j = 0; j < MAX_ITERATION; ++j)
    {
        //clear bias
        for(i = 0; i < body_cnt; ++i)
        {
            refBody = (PyBodyObject*)(PyList_GetItem(world->bodyList, i));
            PyVector2_Set(refBody->cBiasLV, 0.f, 0.f);
            refBody->cBiasW = 0.f;
        }
        //clear impulse
        for(i = 0; i < contact_cnt; ++i)
        {
            contact = (PyJointObject*)(PyList_GetItem(world->contactList, i));
            PyVector2_Set(**(((PyContact*)contact)->ppAccMoment), 0, 0);
            PyVector2_Set(**(((PyContact*)contact)->ppSplitAccMoment), 0, 0);
        }

        //collision reaction
        for(i = 0; i < contact_cnt; ++i)
        {
            contact = (PyJointObject*)(PyList_GetItem(world->contactList, i));
            Collision_ApplyContact((PyObject*)contact, step);
        }
        //update V	
        for(i = 0; i < contact_cnt; ++i)
        {
            contact = (PyJointObject*)(PyList_GetItem(world->contactList, i));
            JointObject_SolveConstraints (contact, step);
        }
    }
}

/**
 * Updates all joints attached to the world.
 *
 * @param world The PyWorldObject to update the joints for.
 * @param stepTime The time passed since the last update.
 */
static void _JointSolve(PyWorldObject* world,double stepTime)
{
    Py_ssize_t size = PyList_Size((PyObject*)(world->jointList));
    Py_ssize_t i;
    PyJointObject* joint;

    for (i = 0; i < size; ++i)
    {
        joint = (PyJointObject*)(PyList_GetItem(world->jointList,i));
        //what happened here?
        if (joint->SolveConstraints)
        {
            JointObject_SolveConstraints (joint, stepTime);
        }
    }
}

/**
 * Updates the positions of the bodies attached to the world.
 *
 * @param world The PyWorldObject to update the body positions for.
 * @param stepTime The time passed since the last update.
 */
static void _BodyPositionUpdate(PyWorldObject* world,double stepTime)
{
    Py_ssize_t size = PyList_Size(world->bodyList);
    Py_ssize_t i;
    PyBodyObject* body;
    for (i = 0; i < size; ++i)
    {
        body = (PyBodyObject*)(PyList_GetItem(world->bodyList,i));
        PyBodyObject_FreeUpdatePos(body,stepTime);	
    }
}

/**
 * Performs any necessary position correction for the attached bodies.
 *
 * @param world The PyWorldObject to update the bodies for.
 * @param stepTime The time passed since the last update.
 */
static void _BodyPositionCorrection(PyWorldObject* world,double stepTime)
{
    Py_ssize_t size = PyList_Size((PyObject*)(world->bodyList));
    Py_ssize_t i;
    PyBodyObject* body;
    for (i = 0; i < size; ++i)
    {
        body = (PyBodyObject*)(PyList_GetItem((PyObject*)(world->bodyList),i));
        PyBodyObject_CorrectPos(body,stepTime);	
    }
}

/**
 * Performs a full world update inluding body updates, collision dection and
 * any necessary correctins.
 *
 * @param world The PyWorldObject to update.
 * @param stepTime The time passed since the last update.
*/
static void _Update(PyWorldObject* world,double stepTime)
{
    int i;
    _FreeBodySimulation(world, stepTime);
    //_PG_BodyPositionUpdate(world, stepTime);
    for(i = 0; i < MAX_ITERATION; ++i)
    {
        _BodyCollisionDetection(world, stepTime);
        _BodyPositionCorrection(world,stepTime);
        _JointSolve(world, stepTime);
    }
    _BodyPositionUpdate(world, stepTime);
	
}

/**
 * Initializes the internals of the passed PyWorldObject with their defaults.
 *
 * @param world The PyWorldObject to initialize.
 */
static void _WorldInit(PyWorldObject* world)
{
    world->bodyList = PyList_New(0);
    world->jointList = PyList_New(0);
    world->contactList = PyList_New(0);
    world->fDamping = 0.0;
    world->fStepTime = 0.1;
    PyVector2_Set(world->vecGravity,0.0,-10);
    world->fTotalTime = 0.0;
}

/**
 * Creates a new PyWorldObject and initializes it.
 */
static PyWorldObject* _WorldNewInternal(PyTypeObject *type)
{
    PyWorldObject* op = (PyWorldObject*)type->tp_alloc(type, 0);
    if (!op)
        return NULL;

    _WorldInit(op);
    return op;
}

/**
 * Creates a new PyWorldObject and initializes it.
 */
static PyObject* _WorldNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    /* In case we have arguments in the python code, parse them later
     * on.
     */
    return (PyObject*) _WorldNewInternal(type);
}

/**
 * Deallocates the passed PyWorldObject.
 *
 * @param world The PyWorldObject to deallocate.
 */
static void _WorldDestroy(PyWorldObject* world)
{
    /*
     * DECREF anything related to the world, such as the lists and
     * release any other memory hold by it.
     */
    Py_XDECREF(world->bodyList);
    Py_XDECREF(world->jointList);
    Py_XDECREF(world->contactList);

    world->ob_type->tp_free((PyObject*)world);
}

/* World getters/setters */

/**
 * Getter for World.gravity.
 */
static PyObject* _World_getGravity(PyWorldObject* world,void* closure)
{
    return Py_BuildValue ("(ff)", world->vecGravity.real,
        world->vecGravity.imag);
}

/**
 * Setter for World.gravity = (x, y)
 */
static int _World_setGravity(PyWorldObject* world,PyObject* value,void* closure)
{
    PyObject *item;
    double real, imag;
    

    if (!PySequence_Check(value) || PySequence_Size (value) != 2)
    {
        PyErr_SetString (PyExc_TypeError, "gravity must be a x, y sequence");
        return -1;
    }

    item = PySequence_GetItem (value, 0);
    if (!DoubleFromObj (item, &real))
    {
        Py_DECREF (item);
        return -1;
    }
    Py_DECREF (item);

    item = PySequence_GetItem (value, 1);
    if (!DoubleFromObj (item, &imag))
    {
        Py_DECREF (item);
        return -1;
    }    
    Py_DECREF (item);

    world->vecGravity.real = real;
    world->vecGravity.imag = imag;
    return 0;
}

/**
 * Getter for World.damping
 */
static PyObject* _World_getDamping(PyWorldObject* world,void* closure)
{
    return PyFloat_FromDouble (world->fDamping);
}

/**
 * Setter for World.damping = x
 */
static int _World_setDamping(PyWorldObject* world,PyObject* value,
    void* closure)
{
    if (PyNumber_Check (value))
    {
        PyObject *tmp = PyNumber_Float (value);

        if (tmp)
        {
            double damping = PyFloat_AsDouble (tmp);
            Py_DECREF (tmp);
            if (PyErr_Occurred ())
                return -1;
            world->fDamping = damping;
            return 0;
        }
    }
    PyErr_SetString (PyExc_TypeError, "damping must be a float");
    return -1;
    
}

/**
 * Getter for World.bodies
 */
static PyObject* _World_getBodyList(PyWorldObject* world,void* closure)
{
    /* Return a copy of the list, so the user cannot manipulate the bodies
     * directly. */
    return PySequence_List (world->bodyList);
}

/**
 * Getter for World.joints
 */
static PyObject* _World_getJointList(PyWorldObject* world,void* closure)
{
    /* Return a copy of the list, so the user cannot manipulate the bodies
     * directly. */
    return PySequence_List (world->jointList);
}


/* World methods */

/**
 * World.update (x)
 */
static PyObject* _World_update (PyWorldObject* world, PyObject* args)
{
    double dt;
    
    if (!PyArg_ParseTuple(args,"|d", &dt))
        dt = 0.1;

    if (dt < 0)
    {
        PyErr_SetString (PyExc_ValueError,
            "step time must not be smaller than 0");
        return NULL;
    }
    _Update (world,dt);
    Py_RETURN_NONE;
}

/**
 * World.add_body (x)
 */
static PyObject* _World_addBody(PyWorldObject* world, PyObject* args)
{
    PyObject* body;
    if (!PyArg_ParseTuple(args,"O",&body))
        return NULL;

    if (!PyBody_Check (body))
    {
        PyErr_SetString(PyExc_ValueError, "argument must be a body");
        return NULL;
    }

    if(!PyWorld_AddBody((PyObject*)world, body))
        return NULL;
    Py_RETURN_NONE;
}

/**
 * World.remove_body (x)
 */
static PyObject* _World_removeBody(PyWorldObject* world, PyObject* args)
{
    PyObject* body;
    if (!PyArg_ParseTuple(args,"O",&body))
        return NULL;

    if (!PyBody_Check (body))
    {
        PyErr_SetString(PyExc_ValueError, "argument must be a body");
        return NULL;
    }

    if(!PyWorld_RemoveBody((PyObject*)world, body))
        return NULL;
    Py_RETURN_NONE;
}

/**
 * World.add_joint (x)
 */
static PyObject* _World_addJoint(PyWorldObject* world,PyObject* args)
{
    PyObject* joint;
    if (!PyArg_ParseTuple(args,"O",&joint))
        return NULL;

    if (!PyJoint_Check (joint))
    {
        PyErr_SetString(PyExc_ValueError,"argument must be a joint");
        return NULL;
    }
    if(!PyWorld_AddJoint((PyObject*)world,joint))
        return NULL;
    Py_RETURN_NONE;
}

/**
 * World.remove_joint (x)
 */
static PyObject* _World_removeJoint(PyWorldObject* world, PyObject* args)
{
    PyObject* joint;
    if (!PyArg_ParseTuple(args,"O",&joint))
        return NULL;

    if (!PyJoint_Check (joint))
    {
        PyErr_SetString(PyExc_ValueError, "argument must be a joint");
        return NULL;
    }

    if(!PyWorld_RemoveJoint((PyObject*)world, joint))
        return NULL;
    Py_RETURN_NONE;
}

/* C API */
static PyObject* PyWorld_New (void)
{
    return _WorldNew(&PyWorld_Type, NULL, NULL);
}

static int PyWorld_AddBody(PyObject* world, PyObject* body)
{
    int contained;

    if (!PyWorld_Check (world))
    {
        PyErr_SetString (PyExc_TypeError, "world must be a World");
        return 0;
    }
 
    if (!PyBody_Check (body))
    {
        PyErr_SetString (PyExc_TypeError, "body must be a Body");
        return 0;
    }

    contained = PySequence_Contains (((PyWorldObject*)world)->bodyList, body);
    if (contained == 1)
    {
        PyErr_SetString (PyExc_ValueError, "body already added");
        return 0;
    }
    else if (contained == -1)
        return 0; /* Error set by the sequence */

    if (PyList_Append(((PyWorldObject*)world)->bodyList, body) == 0)
        return 1;
    return 0;
}

static int PyWorld_RemoveBody(PyObject* world, PyObject* body)
{
    Py_ssize_t _index;

    if (!PyWorld_Check (world))
    {
        PyErr_SetString (PyExc_TypeError, "world must be a World");
        return 0;
    }
 
    if (!PyBody_Check (body))
    {
        PyErr_SetString (PyExc_TypeError, "body must be a Body");
        return 0;
    }

    _index = PySequence_Index (((PyWorldObject*)world)->bodyList, body);
    if (_index != -1)
    {
        if (PySequence_DelItem (((PyWorldObject*)world)->bodyList, _index) == -1)
            return 0; /* Could not delete */
        return 1;
    }
    /* Not in list. */
    return 0;
}

static int PyWorld_AddJoint(PyObject* world, PyObject* joint)
{
    int contained;

    if (!PyWorld_Check (world))
    {
        PyErr_SetString (PyExc_TypeError, "world must be a World");
        return 0;
    }
 
    if (!PyJoint_Check (joint))
    {
        PyErr_SetString (PyExc_TypeError, "joint must be a Joint");
        return 0;
    }
    
    contained = PySequence_Contains (((PyWorldObject*)world)->jointList, joint);
    if (contained == 1)
    {
        PyErr_SetString (PyExc_ValueError, "joint already added");
        return 0;
    }
    else if (contained == -1)
        return 0; /* Error set by the sequence */

    if (PyList_Append(((PyWorldObject*)world)->jointList, joint) == 0)
        return 1;
    return 0;
}

static int PyWorld_RemoveJoint(PyObject* world, PyObject* joint)
{
    Py_ssize_t _index;

    if (!PyWorld_Check (world))
    {
        PyErr_SetString (PyExc_TypeError, "world must be a World");
        return 0;
    }
 
    if (!PyJoint_Check (joint))
    {
        PyErr_SetString (PyExc_TypeError, "joint must be a Joint");
        return 0;
    }

    _index = PySequence_Index (((PyWorldObject*)world)->jointList, joint);
    if (_index != -1)
    {
        if (PySequence_DelItem (((PyWorldObject*)world)->jointList, _index) == -1)
            return 0; /* Could not delete */
        return 1;
    }
    /* Not in list. */
    return 0;
}

static int PyWorld_Update(PyObject* world, double dt)
{
    if (!PyWorld_Check (world))
    {
        PyErr_SetString (PyExc_TypeError, "world must be a World");
        return 0;
    }

    _Update ((PyWorldObject*)world, dt);
    return 1;
}

void PyWorldObject_ExportCAPI (void **c_api)
{
    c_api[PHYSICS_WORLD_FIRSTSLOT] = &PyWorld_Type;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 1] = &PyWorld_New;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 2] = &PyWorld_AddBody;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 3] = &PyWorld_RemoveBody;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 4] = &PyWorld_AddJoint;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 5] = &PyWorld_RemoveJoint;
    c_api[PHYSICS_WORLD_FIRSTSLOT + 6] = &PyWorld_Update;
}
