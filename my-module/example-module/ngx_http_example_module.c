#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>

typedef struct {
  ngx_str_t        my_str;
  ngx_int_t        my_num;
  ngx_flag_t       my_flag;
  size_t           my_size;
  ngx_array_t      *my_str_array;
  ngx_array_t      *my_keyval;
  off_t            my_off;
  ngx_msec_t       my_msec;
  time_t           my_sec;
  ngx_bufs_t       my_bufs;
  ngx_uint_t       my_enum_seq;
  ngx_uint_t       my_bitmask;
  ngx_uint_t       my_access;
  ngx_path_t       *my_path;
} ngx_http_example_conf_t;

static ngx_conf_enum_t test_enums[] = {
  {ngx_string("apple"), 1},
  {ngx_string("banana"), 2},
  {ngx_string("orange"), 3},
  {ngx_null_string, 0}
};

static ngx_conf_bitmask_t test_bitmasks[] = {
  {ngx_string("good"), 0x0002},
  {ngx_string("better"), 0x0004},
  {ngx_string("best"), 0x0008},
  {ngx_null_string, 0}
};

static void *ngx_http_example_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_example_conf_t *mycf;
  mycf = ngx_pcalloc(cf->pool, sizeof(ngx_http_example_conf_t));
  if (mycf == NULL)
  {
    return NULL;
  }

  mycf->my_flag = NGX_CONF_UNSET;
  mycf->my_num = NGX_CONF_UNSET;
  mycf->my_str_array = NGX_CONF_UNSET_PTR;
  mycf->my_keyval = NULL;
  mycf->my_off = NGX_CONF_UNSET;
  mycf->my_msec = NGX_CONF_UNSET_MSEC;
  mycf->my_sec = NGX_CONF_UNSET;
  mycf->my_size = NGX_CONF_UNSET_SIZE;

  return mycf;
}

static ngx_int_t ngx_http_example_handler(ngx_http_request_t *r)
{
  //必须是GET OR HEAD方法，否则返回405 Not Allowed
  if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD)))
  {
    return NGX_HTTP_NOT_ALLOWED;
  }

  //丢弃请求中的包体
  ngx_int_t rc = ngx_http_discard_request_body(r);
  if (rc != NGX_OK)
  {
    return rc;
  }

  //设置返回的Content-Type.注意，ngx_str_t有一个很方便的初始化宏ngx_string，它可以把ngx_str_t的data和len成员都设置好
  ngx_str_t type = ngx_string("text/plain");
  //返回的包体内容
  ngx_str_t response = ngx_string("parse_conf");
  //设置返回状态码
  r->headers_out.status = NGX_HTTP_OK;
  //响应包是有包体内容的，需要设置Content-Length长度
  r->headers_out.content_length_n = response.len;
  //设置Content-Type
  r->headers_out.content_type = type;

  //发送HTTP头部
  rc = ngx_http_send_header(r);
  if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
  {
    return rc;
  }

  //构造 ngx_buf_t结构体准备发送包体
  ngx_buf_t *b;
  b = ngx_create_temp_buf(r->pool, response.len);
  if (b == NULL)
  {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }
  //将Hello World 复制到 ngx_buf_t指向的内存中
  ngx_memcpy(b->pos, response.data, response.len);
  //注意, 一定要设置好last指针
  b->last = b->pos + response.len;
  //声明这是最后一块缓冲区
  b->last_buf = 1;

  //构造发送时的ngx_chain_t结构体
  ngx_chain_t out;
  //赋值ngx_buf_t
  out.buf = b;
  //设置next为NULL
  out.next = NULL;
  //发送包体，发送结束后HTTP框架会调用ngx_http_finalize_request方法结束请求
  return ngx_http_output_filter(r, &out);
}

static char * ngx_http_example(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;
  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_example_handler;

  return NGX_CONF_OK;
}


static ngx_command_t ngx_http_example_command[] = {
  {
    ngx_string("test_flag"),
    NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_flag),
    NULL
  },
  {
    ngx_string("test_str"),
    NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_str),
    NULL
  },
  {
    ngx_string("test_str_array"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_str_array_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_str_array),
    NULL
  },
  {
    ngx_string("test_keyval"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
    ngx_conf_set_keyval_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_keyval),
    NULL
  },
  {
    ngx_string("test_num"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
    ngx_conf_set_num_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_num),
    NULL
  },
  {
    ngx_string("test_size"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_size_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_size),
    NULL
  },
  {
    ngx_string("test_off"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_off_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_off),
    NULL
  },
  {
    ngx_string("test_msec"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_msec),
    NULL
  },
  {
    ngx_string("test_sec"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_sec_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_sec),
    NULL
  },
  {
    ngx_string("test_bufs"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
    ngx_conf_set_bufs_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_bufs),
    NULL
  },
  {
    ngx_string("test_enum"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_enum_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_enum_seq),
    test_enum
  },
  {
    ngx_string("test_bitmask"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_bitmask_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_bitmask),
    test_bitmasks
  },
  {
    ngx_string("test_access"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE123,
    ngx_conf_set_access_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_access),
    NULL
  },
  {
    ngx_string("test_path"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1234,
    ngx_conf_set_path_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_example_conf_t, my_path),
    NULL
  },
  {
    ngx_string("test_noargs"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_NOARGS,
    ngx_http_example,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL
  },

  ngx_null_command
};

static ngx_http_module_t ngx_http_example_module_ctx = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  ngx_http_example_create_loc_conf,
  NULL
};

ngx_module_t ngx_http_example_module = {
  NGX_MODULE_V1,
  &ngx_http_example_module_ctx,
  ngx_http_example_command,
  NGX_HTTP_MODULE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};
