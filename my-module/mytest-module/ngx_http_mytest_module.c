#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>
#include <ngx_http_upstream.h>

//#include <ngx_conf_file.h>
//#include <ngx_http_config.h>
//#include <ngx_http_request.h>
typedef struct {
  ngx_str_t my_config_str;
  ngx_int_t my_config_num;

  ngx_http_upstream_conf_t upstream;
} ngx_http_mytest_conf_t;

typedef struct {
  ngx_http_status_t status;
} ngx_http_mytest_ctx_t;

static ngx_int_t mytest_upstream_process_header(ngx_http_request_t *r)
{
  ngx_int_t rc;
  ngx_table_elt_t *h;
  ngx_http_upstream_header_t *hh;
  ngx_http_upstream_main_conf_t *umcf;

  /*这里将upstream模块配置项ngx_http_upstream_main_conf_t取出来，目的只有一个，就是对将
    要转发给下游客户端的HTTP响应头部进行统一处理，该结构体中存储了需要进行统一处理的HTTP头部
    名称和回调方法*/
  umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

  /*循环地解析所有的HTTP头部*/
  for ( ;; )
  {
    /*HTTP框架提供了基础性的ngx_http_parse_header_line方法，它用于解析HTTP头部*/
    rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
    /*返回NGX_OK时，表示解析出一行HTTP头部*/
    if (rc == NGX_OK)
    {
      /*向headers_in.headers这个ngx_list_t链表中添加HTTP头部*/
      h = ngx_list_push(&r->upstream->headers_in.headers);
      if (h == NULL)
      {
        return NGX_ERROR;
      }
      /*下面开始构造刚刚添加到headers链表中的HTTP头部*/
      h->hash = r->header_hash;

      h->key.len = r->header_name_end - r->header_name_start;
      h->value.len = r->header_end - r->header_start;
      /*必须在内存池中分配存放HTTP头部的内存空间*/
      h->key.data = ngx_pnalloc(r->pool, h->key.len + 1 + h->value.len + 1 + h->key.len);
      if (h->key.data == NULL)
      {
        return NGX_ERROR;
      }

      h->value.data = h->key.data + h->key.len + 1;
      h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;

      ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
      h->key.data[h->key.len] = '\0';
      ngx_memcpy(h->value.data, r->header_start, h->value.len);
      h->value.data[h->value.len] = '\0';

      if (h->key.len == r->lowcase_index)
      {
        ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
      }
      else
      {
        ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
      }

      /*upstream模块会对一些HTTP头部做特殊处理*/
      hh = ngx_hash_find(&umcf->headers_in_hash, h->hash, h->lowcase_key, h->key.len);

      if (hh && hh->handler(r, h, hh->offset) != NGX_OK)
      {
        return NGX_ERROR;
      }

      continue;
    }

    /*返回NGX_HTTP_PARSE_HEADER_DONE时，表示响应中所有的HTTP头部都解析完毕，接下来再接收到的都将是HTTP宝体*/
    if (rc == NGX_HTTP_PARSE_HEADER_DONE)
    {
      /*如果之前解析HTTP头部时没有发现server和date头部，那么下面会根据HTTP协议规范添加这两个头部*/
      if (r->upstream->headers_in.server == NULL)
      {
        h = ngx_list_push(&r->upstream->headers_in.headers);
        if (h == NULL)
        {
          return NGX_ERROR;
        }

        h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');
        ngx_str_set(&h->key, "Server");
        ngx_str_null(&h->value);
        h->lowcase_key = (u_char *) "server";
      }

      if (r->upstream->headers_in.date == NULL)
      {
        h = ngx_list_push(&r->upstream->headers_in.headers);
        if (h == NULL)
        {
          return NGX_ERROR;
        }

        h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');

        ngx_str_set(&h->key, "Date");
        ngx_str_null(&h->value);
        h->lowcase_key = (u_char *) "date";
      }

      return NGX_OK;
    }

    /*如果返回NGX_AGAIN，则表示状态机还没有解析到完整的HTTP头部，此时要求upstream模块继续接收新的字符流，然后
     交由process_header回调方法解析*/
    if (rc == NGX_AGAIN)
    {
      return NGX_AGAIN;
    }

    /*其他返回值都是非法的*/
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "upstream sent invalid header");

    return NGX_HTTP_UPSTREAM_INVALID_HEADER;
  }
}

static ngx_int_t mytest_process_status_line(ngx_http_request_t *r)
{
  size_t len;
  ngx_int_t rc;
  ngx_http_upstream_t *u;

  /*上下文中才会保存多次解析HTTP响应行的状态，下面首先取出请求的上下文*/
  ngx_http_mytest_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_mytest_module);
  if (ctx == NULL)
  {
    return NGX_ERROR;
  }

  u = r->upstream;

  /*HTTP框架提供的ngx_http_parse_status_line方法可以解析HTTP响应行，它的输入就是收到的字符流
    和上下文种的ngx_http_status_t结构*/
  rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);
  /*返回NGX_AGAIN时，表示还没有解析出完整的HTTP响应行，需要接收更多的字符流再进行解析*/
  if (rc == NGX_AGAIN)
  {
    return rc;
  }
  /*返回NGX_ERROR时，表示没有接收到合法的HTTP响应行*/
  if (rc == NGX_ERROR)
  {
    ngx_log_error();

    r->http_version = NGX_HTTP_VERSION_9;
    u->state->status = NGX_HTTP_OK;

    return NGX_OK;
  }

  /*以下表示在解析到完整的HTTP响应行时，会做一些简单地赋值操作，将解析出的信息设置到r->upstream->headers_in
    结构体中。当upstream解析完所有的包头时，会把headers_in中的成员设置到将要向下游发送的r->headers_out结构体
    中，也就是说，现在用户向headers_in中设置的信息，最终都会发往下游客户端。为什么不直接设置r->headers_out而
    要多此一举呢？因为upstream希望能够按照ngx_http_upstream_conf_t配置结构体中的hide_headers等成员对发往
    下游的响应头部做统一处理*/
  if (u->state)
  {
    u->state->status = ctx->status.code;
  }

  u->headers_in.status_n = ctx->status.code;

  len = ctx->status.end - ctx->status.start;
  u->headers_in.status_line.len = len;

  u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
  if (u->headers_in.status_line.data == NULL)
  {
    return NGX_ERROR;
  }

  ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);

  /*下一步将开始解析HTTP头部。设置process_header回调方法为mytest_upstream_process_header，之后再收到
    的新字符流将由mytest_upstream_process_header解析*/
  u->process_header = mytest_upstream_process_header;

  /*如果本次收到的字符流除了HTTP响应行外，还有多余的字符，那么将由mytest_upstream_process_header方法解析*/
  return mytest_upstream_process_header(r);
}

static ngx_int_t mytest_upstream_create_request(ngx_http_request_t *r)
{
  /*发往google上游服务器的请求很简单，就是模仿正常的搜索请求，以/search?q=...的url来发起搜索请求*/
  static ngx_str_t backendQueryLine = ngx_string("GET /search?q=%V HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n");
  ngx_int_t queryLineLen = backendQueryLine.len + r->args.len - 2;
  /*内存池申请内存*/
  ngx_buf_t *b = ngx_create_temp_buf(r->pool, queryLineLen);
  if (b == NULL)
  {
    return NGX_ERROR;
  }
  /*last要指向请求的末尾*/
  b->last = b->pos + queryLineLen;

  /*snprintf*/
  ngx_snprintf(b->pos, queryLineLen, (char *)backendQueryLine.data, &r->args);
  /*ngx_chain_t结构，包含发送给上游服务器的请求*/
  r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
  if (r->upstream->request_bufs == NULL)
  {
    return NGX_ERROR;
  }

  /*在这里只包含1个ngx_buf_t缓冲区*/
  r->upstream->request_bufs->buf = b;
  r->upstream->request_bufs->next = NULL;

  r->upstream->request_sent = 0;
  r->upstream->header_sent = 0;

  //header_hash不可以为0
  r->header_hash = 1;

  return NGX_OK;
}

static void mytest_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
}

static ngx_int_t ngx_http_myconfig_handler(ngx_http_request_t *r)
{
  int mode = 0;
  if (mode == 0)
  {
    //将配置项的upstream赋值给ngx_http_upstream_t中的conf成员
    ngx_http_mytest_conf_t *mycf = (ngx_http_mytest_conf_t *) ngx_http_get_module_loc_conf(r, ngx_http_mytest_module);
    r->upstream->conf = mycf->upstream;
    r->upstream->create_request = mytest_upstream_create_request;
    r->upstream->process_header = mytest_process_status_line;
    r->upstream->finalize_request = mytest_finalize_request;

    //引用计数，HTTP框架只有在引用技术为0时才能真正地销毁请求
    r->main->count++;
    //启动upstream机制
    ngx_http_upstream_init(r);

    return NGX_DONE;
  }
  else
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
}

static char *ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  /*注意，参数conf就是HTTP框架传给用户的在ngx_http_mytest_create_loc_conf回调方法
   中分配的结构体ngx_http_mytest_conf_t*/
  ngx_http_mytest_conf_t *mycf = conf;
  /*cf->args是1个ngx_array_t队列，它的成员都是ngx_str_t结构。
    我们用value指向ngx_array_t的elts内容，其中value[1]就是第1个参数，同理，value[2]是第2个参数*/
  ngx_str_t *value = cf->args->elts;
  /**/
  if (cf->args->nelts > 1)
  {
    mycf->my_config_str = value[1];
  }

  if (cf->args->nelts > 2)
  {
    mycf->my_config_num = ngx_atoi(value[2].data, value[2].len);
    if (mycf->my_config_num == NGX_ERROR)
    {
      return "invalid number";
    }
  }

  /* ngx_http_core_loc_conf_t *clcf; */
  /* clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module); */
  /* clcf->handler = ngx_http_myconfig_handler; */

  return NGX_CONF_OK;
}

static void *ngx_http_mytest_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_mytest_conf_t *mycf;
  mycf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mytest_conf_t));
  if (mycf == NULL)
  {
    return NULL;
  }

  /*以下简单的硬编码ngx_http_upstream_conf_t结构中的各成员，如超时时间，都设为1分钟，这也是
    HTTP反向代理模块的默认值*/
  mycf->my_config_num = NGX_CONF_UNSET;
  mycf->upstream.connect_timeout = 60000;
  mycf->upstream.send_timeout = 60000;
  mycf->upstream.read_timeout = 60000;
  mycf->upstream.store_access = 0600;

  /*实际上，buffering已经决定了将以固定大小的内存作为i缓冲区来转发上游的响应包体，这块固定缓冲区的
    大小就是buffer_size。如果buffering为1，就会使用更多的内存缓存来不及发往下游的响应。例如，最
    多使用bufs.num个缓冲区且每个缓冲区大小为bufs.size。另外，还会使用临时文件，临时文件的最大长度
    为max_temp_file_size*/
  mycf->upstream.buffering = 0;
  mycf->upstream.bufs.num = 8;
  mycf->upstream.bufs.size = ngx_pagesize;
  mycf->upstream.buffer_size = ngx_pagesize;
  mycf->upstream.busy_buffers_size = 2 * ngx_pagesize;
  mycf->upstream.temp_file_write_size = 2 * ngx_pagesize;
  mycf->upstream.max_temp_file_size = 1024 * 1024 * 1024;

  /*upstream模块要求hide_headers成员必须要初始化(upstream在解析完上游服务器返回的包头时，会调用
    ngx_http_upstream_process_headers方法按照hide_headers成员将本应转发给下游的一些HTTP头部
    隐藏)，这里将它赋为NGX_CONF_UNSET_PTR，这是为了在merge合并配置项方法中使用upstream模块提供
    的ngx_http_upstream_hide_headers_hash方法初始化hide_headers成员*/
  mycf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
  mycf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

  return mycf;
}

static char * ngx_http_mytest_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_mytest_conf_t *prev = (ngx_http_mytest_conf_t *)parent;
  ngx_http_mytest_conf_t *conf = (ngx_http_mytest_conf_t *)child;

  ngx_hash_init_t hash;
  hash.max_size = 100;
  hash.bucket_size = 1024;
  hash.name = "proxy_headers_hash";
  if (ngx_http_upstream_hide_headers_hash(cf,
                                          &conf->upstream,
                                          &prev->upstream,
                                          ngx_http_proxy_hide_headers,
                                          &hash) != NGX_OK)
  {
    return NGX_CONF_ERROR;
  }

      //ngx_conf_merge_str_value(conf->my_config_str, prev->my_config_num, "defaultstr");

  return NGX_CONF_OK;
}

static ngx_command_t ngx_http_mytest_command[] = {
  {
    ngx_string("test_myconfig"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE12,
    ngx_conf_set_myconfig,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL
  },
  {
    ngx_string("upstream_connect_timeout"),
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_mytest_conf_t, upstream.connect_timeout)
  },

  ngx_null_command
};

static ngx_http_module_t ngx_http_mytest_module_ctx = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  ngx_http_mytest_create_loc_conf,
  ngx_http_mytest_merge_loc_conf
};

ngx_module_t ngx_http_mytest_module = {
  NGX_MODULE_V1,
  &ngx_http_mytest_module_ctx,
  ngx_http_mytest_command,
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
