/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-9-18-上午9:57:07             1.0             build this file
 ********************************************************************************/
/*!
 * \file     dsp.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-9-18-上午9:57:07
 *******************************************************************************/

#ifndef DSP_H_
#define DSP_H_

#include <kernel.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dsp_mbox_param{
    u16_t cmd;  //最高bit方向 7bit组号 8bit消息ID
    u16_t idx;
    u32_t wait;
    s32_t result;
    u32_t param;
    u32_t size;
};

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in
 * public documentation.
 */
typedef void (*dsp_callback_t)(struct dsp_mbox_param *param);

typedef int (*dsp_api_poweron)(struct device *dev);

typedef int (*dsp_api_poweroff)(struct device *dev);

typedef int (*dsp_api_suspend)(struct device *dev);

typedef int (*dsp_api_resume)(struct device *dev);

typedef int (*dsp_api_set_rate)(struct device *dev, int *rate);

typedef int (*dsp_api_register_callback)(struct device *dev, int chan, dsp_callback_t dsp_callback);

typedef int (*dsp_api_unregister_callback)(struct device *dev, int chan);

typedef int (*dsp_api_load_image)(struct device *dev, const char *file_name, int type);

typedef int (*dsp_api_free_image)(struct device *dev, int type);

typedef int (*dsp_api_request_image)(struct device *dev, unsigned int code_address);

typedef int (*dsp_api_send_message)(struct device *dev, struct dsp_mbox_param *param);

typedef int (*dsp_api_response_done)(struct device *dev, struct dsp_mbox_param *param);

struct dsp_driver_api {
	dsp_api_poweron poweron;
	dsp_api_poweroff poweroff;
	dsp_api_suspend suspend;
	dsp_api_resume resume;
	dsp_api_set_rate set_rate;
	dsp_api_register_callback register_callback;
	dsp_api_unregister_callback unregister_callback;
    dsp_api_load_image load_image;
	dsp_api_free_image free_image;
	dsp_api_request_image request_image;
	dsp_api_send_message send_message;
	dsp_api_response_done response_done;
};
/**
 * @endcond
 */

/**
 * @brief start the dsp device
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_poweron(struct device *dev)
{
	const struct dsp_driver_api *api = dev->driver_api;

	return api->poweron(dev);
}

/**
 * @brief stop the dsp device
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_poweroff(struct device *dev)
{
	const struct dsp_driver_api *api = dev->driver_api;

	return api->poweroff(dev);
}

/**
 * @brief suspend the dsp device
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_suspend(struct device *dev)
{
	const struct dsp_driver_api *api = dev->driver_api;

	return api->suspend(dev);
}

/**
 * @brief resume the dsp device
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_resume(struct device *dev)
{
	const struct dsp_driver_api *api = dev->driver_api;

	return api->resume(dev);
}

/**
 * @brief set the dsp device clock rate
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_set_rate(struct device *dev, int *rate)
{
	const struct dsp_driver_api *api = dev->driver_api;

	return api->set_rate(dev, rate);
}


/**
 * @brief load the dsp image
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param file_name dsp libary filename
 * @param type dsp libary filetype
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_load_image(struct device *dev, const char *file_name, int type)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->load_image(dev, file_name, type);
}

/**
 * @brief unload the dsp image
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param type dsp libary filetype
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_free_image(struct device *dev, int type)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->free_image(dev, type);
}

/**
 * @brief request the dsp image
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param epc     dsp code address
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_request_image(struct device *dev, unsigned int code_address)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->request_image(dev, code_address);
}

/**
 * @brief register dsp callback
 *
 * @param dev           Pointer to the device structure for the driver instance.
 * @param type          the datapath type
 * @param dsp_callback  datapath callback function
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_register_callback(struct device *dev, int type, dsp_callback_t dsp_callback)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->register_callback(dev, type, dsp_callback);
}

/**
 * @brief unregister dsp callback
 *
 * @param dev           Pointer to the device structure for the driver instance.
 * @param type          the datapath type
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_unregister_callback(struct device *dev, int type)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->unregister_callback(dev, type);
}

/**
 * @brief send message to dsp
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param wait    wait for dsp response the message request
 * @param chan    datapath num
 * @param cmd     message command
 * @param param   message param
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_send_message(struct device *dev, struct dsp_mbox_param *param)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->send_message(dev, param);
}

/**
 * @brief send response to dsp
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful.
 * @retval Negative errno code if failure.
 */
static inline int dsp_response_done(struct device *dev, struct dsp_mbox_param *param)
{
    const struct dsp_driver_api *api = dev->driver_api;

    return api->response_done(dev, param);
}

int dsp_acts_start(struct device *dev);
int dsp_acts_stop(struct device *dev);
int dsp_acts_ready_start(struct device *dev, void *dsp_comm_irq_handler);
int dsp_acts_init(struct device *dev);
int respond_dsp(void);
int dsp_acts_open(struct device *dev, const char *file_name, int type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /* DSP_H_ */
