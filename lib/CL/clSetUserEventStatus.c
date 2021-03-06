#include "pocl_cl.h"
CL_API_ENTRY cl_int CL_API_CALL
POname(clSetUserEventStatus)(cl_event    event ,
                     cl_int      execution_status ) CL_API_SUFFIX__VERSION_1_1
{
  /* Must be a valid user event */
  if (event == NULL || event->command_type != CL_COMMAND_USER)
    return CL_INVALID_EVENT;
  /* Can only be set to CL_COMPLETE (0) or negative values */
  if (execution_status > CL_COMPLETE)
    return CL_INVALID_VALUE;
  /* Can only be done once */
  if (event->status <= CL_COMPLETE)
    return CL_INVALID_OPERATION;

  event->status = execution_status;

  return CL_SUCCESS;
}
POsym(clSetUserEventStatus)
