.. lua:method:: addeventhandler(func)

    Add an event handler for this UI element. See :ref:`ui-events`.

    :param function func:
    :rtype: integer
    :return: A id that can be used with :lua:meth:`removeeventhandler`

    .. versionhistory::
        :0.1.0: Added

.. lua:method:: removeeventhandler(id)
    
    Remove an event handler for this UI element.

    :param integer id:

    .. versionhistory::
        :0.1.0: Added
