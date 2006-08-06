#!/usr/bin/env python

'''
Mangle Pygame source to remove names we don't want appearing in the
documentation.

Usage:
  # Generate source code for pygame module in build_doc:
  python support/prep_doc_pygame.py build_doc/

This script is called automatically from setup.py.
'''

__docformat__ = 'restructuredtext'
__version__ = '$Id$'

import ctypes
import inspect
import os.path
import sys
import StringIO

base_dir = os.path.abspath(sys.argv[1])
modules = {}
done_modules = []

def write_function(func, file, indent=''):
    if hasattr(func, '__doc__') and func.__doc__:
        docstring = func.__doc__
    else:
        docstring = ''
    if hasattr(func, '_args'):
        args = func._args
    else:
        args, foo, foo, defaults = inspect.getargspec(func)
        defaults = defaults or []
        nondefault = len(args) - len(defaults)
        args = args[:nondefault] + \
            ['%s=%r' % (args[i+nondefault], defaults[i]) \
                for i in range(len(defaults))]
    print >> file, '%sdef %s(%s):' % (indent, func.__name__, ','.join(args))
    print >> file, "%s    '''%s'''" % (indent, docstring)
    print >> file

def write_class(cls, file):
    if ctypes.Structure in cls.__bases__ or \
       ctypes.Union in cls.__bases__:
        print >> file, 'class %s:' % cls.__name__
        print >> file, '    %s' % repr(cls.__doc__ or '')
        for func in dir(cls):
            if func[0] == '_' and \
               (func[:2] != '__' or func in ('__getattr__', '__setattr__')):
                continue
            func = getattr(cls, func)
            if inspect.ismethod(func):
                write_function(func, file, '    ')

        #for field in cls._fields_:
        #    if field[0] != '_':
        #        print >> file, '    %s = None' % field[0]
    else:
        print >> file, '%s' % inspect.getsource(cls)

def write_variable(child_name, child, file):
    print >> file, '%s = %s' % (child_name, repr(child))

def write_module(module):
    done_modules.append(module)
    f = module_file(module.__name__)
    if not f:
        return

    if module is pygame.sprite:
        f.write(inspect.getsource(module))
        return

    print >> f, "'''%s'''\n" % module.__doc__
    print >> f, '__docformat__ = "restructuredtext"'
    for child_name in dir(module):
        # Ignore privates
        if child_name[0] == '_':
            continue
        
        child = getattr(module, child_name)
        child_module = inspect.getmodule(child) or module
        if child_module is not module:
            if child_module not in done_modules:
                write_module(child_module)
            if module not in (pygame, pygame.locals):
                continue
        if child_module.__name__[:6] != 'pygame':
            continue

        if inspect.isfunction(child) and child_name[0] != '_':
            write_function(child, f)
        elif inspect.isclass(child):
            write_class(child, f)
        elif inspect.ismodule(child):
            pass
        elif module in (pygame.locals, pygame.constants):
            write_variable(child_name, child, f)

def module_file(module_name):
    if module_name[:6] != 'pygame' or \
       module_name in ['pygame.colordict', 'pygame.array', 'pygame.base',
                       'pygame.sysfont', 'pygame.version']:
        return None

    if module_name in modules:
        return modules[module_name]
    else:
        f = open(os.path.join(base_dir, 
                              '%s.py' % module_name.replace('.', '/')), 'w')
        modules[module_name] = f
        return f

if __name__ == '__main__':
    try:
        os.makedirs(os.path.join(base_dir, 'pygame'))
    except:
        pass
    modules['pygame'] = open(os.path.join(base_dir, 'pygame/__init__.py'), 'w')
    import pygame
    write_module(pygame)

