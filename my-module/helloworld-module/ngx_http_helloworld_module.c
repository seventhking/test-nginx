#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>

//#include <ngx_conf_file.h>
//#include <ngx_http_config.h>
//#include <ngx_http_request.h>

static ngx_int_t ngx_http_helloworld_handler(ngx_http_request_t *r)
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
  ngx_str_t response = ngx_string("Hello World");
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


static char * ngx_http_helloworld(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;
  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_helloworld_handler;

  return NGX_CONF_OK;
}

static ngx_command_t ngx_http_helloworld_command[] = {
  {
    ngx_string("helloworld"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_NOARGS,
    ngx_http_helloworld,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL
  },

  ngx_null_command
};

static ngx_http_module_t ngx_http_helloworld_module_ctx = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

ngx_module_t ngx_http_helloworld_module = {
  NGX_MODULE_V1,
  &ngx_http_helloworld_module_ctx,
  ngx_http_helloworld_command,
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
