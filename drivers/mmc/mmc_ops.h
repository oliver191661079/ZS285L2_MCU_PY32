#ifndef __MMC_OPS_H__
#define __MMC_OPS_H__

int mmc_select_card(struct device *mmc_dev, struct mmc_cmd *cmd, int rca);
int mmc_go_idle(struct device *mmc_dev, struct mmc_cmd *cmd);
int mmc_send_status(struct device *mmc_dev, struct mmc_cmd *cmd, int rca, u32_t *status);
int mmc_app_cmd(struct device *mmc_dev, struct mmc_cmd *cmd, int rca);
int mmc_send_app_cmd(struct device *mmc_dev, int rca, struct mmc_cmd *cmd,
		     int retries);
int mmc_all_send_cid(struct device *mmc_dev, struct mmc_cmd *cmd, u32_t *cid);
int mmc_send_csd(struct device *mmc_dev, struct mmc_cmd *cmd, int rca, u32_t *csd);
int mmc_send_ext_csd(struct device *mmc_dev, struct mmc_cmd *cmd, u8_t *ext_csd);
int mmc_set_blockcount(struct device *mmc_dev, struct mmc_cmd *cmd, unsigned int blockcount,
		       bool is_rel_write);
int mmc_stop_block_transmission(struct device *mmc_dev, struct mmc_cmd *cmd);
int mmc_send_relative_addr(struct device *mmc_dev, struct mmc_cmd *cmd, unsigned int *rca);
int emmc_send_relative_addr(struct device *mmc_dev, struct mmc_cmd *cmd, unsigned int *rca);

#endif	/* __MMC_OPS_H__ */
